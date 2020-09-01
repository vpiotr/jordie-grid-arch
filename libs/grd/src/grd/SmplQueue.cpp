/////////////////////////////////////////////////////////////////////////////
// Name:        SmplQueue.cpp
// Project:     grdLib
// Purpose:     Implementation of simple queue. See header file.
// Author:      Piotr Likus
// Modified by:
// Created:     15/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//std
#include <memory>
#include <set>

//boost
#include "boost/ptr_container/ptr_map.hpp"
#include "boost/ptr_container/ptr_list.hpp"

//sc
#include "sc/utils.h"
#include "sc/dtypes.h"
#include "perf/Log.h"
#include "perf/time_utils.h"
#include "perf/Timer.h"
#include "perf/Counter.h"

//grd
#include "grd/SmplQueue.h"
#include "grd/MessageConst.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

#define SMPL_QUEUE_TIMERS_ENABLED
#define SMPL_QUEUE_COUNTERS_ENABLED

using namespace perf;

//#define SMPL_QUEUE_LOG_ENABLED
const uint DEF_QUEUE_VALIDATE_DELAY = 50;

// ----------------------------------------------------------------------------
// Local class declarations
// ----------------------------------------------------------------------------
/// deletes messages automatically without sending anywhere
class scSmplQueueManagerTaskNullDev: public scSmplQueueManagerTask {
public:
  scSmplQueueManagerTaskNullDev(): scSmplQueueManagerTask(true) {};
  virtual ~scSmplQueueManagerTaskNullDev() {};
  virtual int handleMessage(scEnvelope &envelope, scResponse &response);
};

/// only for read on request, does not send automatically messages
class scSmplQueueManagerTaskPull: public scSmplQueueManagerTask {
public:
  scSmplQueueManagerTaskPull(bool allowSenderAsReader): scSmplQueueManagerTask(allowSenderAsReader) {};
  virtual ~scSmplQueueManagerTaskPull() {};
  virtual void addReader(scSmplQueueReaderTask *reader);
  virtual bool hasMessageForReader(scSmplQueueReaderTask *reader);
};

/// sends each message to every connected reader
class scSmplQueueManagerTaskMultiCast: public scSmplQueueManagerTask {
public:
  scSmplQueueManagerTaskMultiCast(bool allowSenderAsReader): scSmplQueueManagerTask(allowSenderAsReader) {};
  virtual ~scSmplQueueManagerTaskMultiCast() {};
  virtual bool get(scEnvelope &a_envelope);
  virtual int handleMessage(scEnvelope &envelope, scResponse &response);
  virtual bool needsRun();
  virtual int intRun();
  virtual bool hasMessageForReader(scSmplQueueReaderTask *reader);
};

class scDurableRequestInfo
{
public:
  scDurableRequestInfo(); 
  scDurableRequestInfo(const scDurableRequestInfo& rhs);            // copy constructor
  scDurableRequestInfo& operator=(const scDurableRequestInfo& rhs); // copy assignment operator
  virtual ~scDurableRequestInfo();
  cpu_ticks getStartTime() const; 
  uint getRetryCount();
  void incRetryCount();
  void resetStartTime();
  void setStartTime(cpu_ticks ticks);
  bool isTimeToStart();
  void setReaderName(const scString &name);
  const scString &getReaderName();
protected:
  void init();  
protected:
  cpu_ticks m_startTime;
  cpu_ticks m_initTime;
  uint m_retryCount;
  scString m_readerName;
};

typedef boost::ptr_map<uint,scDurableRequestInfo> scDurableRequestInfoMap;

/// sends message to first available reader
class scSmplQueueManagerTaskDurable: public scSmplQueueManagerTask {
  typedef scSmplQueueManagerTask inherited;
public:
  // create
  scSmplQueueManagerTaskDurable(bool allowSenderAsReader, bool durable): 
    scSmplQueueManagerTask(allowSenderAsReader), 
    m_durable(durable), m_retryLimit(0), m_retryDelay(0), m_contactTimeout(0), m_resultTimeout(0), m_storeTimeout(0) 
    {};
  virtual ~scSmplQueueManagerTaskDurable() {};
  // properties
  void setRetryLimit(uint value);
  void setRetryDelay(cpu_ticks value);
  cpu_ticks getRetryDelay();
  void setContactTimeout(cpu_ticks value);
  void setResultTimeout(cpu_ticks value);
  void setStoreTimeout(cpu_ticks value);
  // run
  virtual bool get(scEnvelope &a_envelope);
  virtual void put(const scEnvelope &envelope);  
  virtual bool handleReaderResponse(
    scSmplQueueReaderTask &reader, const scString &readerTarget, 
    const scEnvelope &envelope, const scResponse &response
  );
  virtual void handleEnvelopeSent(scSmplQueueReaderTask *reader, const scEnvelope &envelope);
  bool handleRequestError(uint reqId, const scEnvelope &envelope, const scResponse &response);
protected:  
  void putRetry(const scEnvelope &envelope);
  void intPut(const scEnvelope &envelope, bool retry);
  uint getRetryCount(uint reqId);
  void clearRequestInfo(uint reqId);
  void prepareRetry(uint reqId, const scEnvelope &envelope);
  void validateReaders();
  void validateRequests();
  virtual int intRun();
  bool sendRequestFailed(uint reqId, int statusCode);
  bool sendRequestFailed(uint reqId, const scResponse &response);
  void eraseFromWaiting(uint reqId);
protected:
  bool m_durable;  
  uint m_retryLimit;
  cpu_ticks m_retryDelay;
  cpu_ticks m_contactTimeout;
  cpu_ticks m_resultTimeout;
  cpu_ticks m_storeTimeout;
  scDurableRequestInfoMap m_requestMap;
};

/// send message to next available reader
class scSmplQueueManagerTaskRoundRobin: public scSmplQueueManagerTaskDurable {
  typedef scSmplQueueManagerTaskDurable inherited;
public:
  // create
  scSmplQueueManagerTaskRoundRobin(bool allowSenderAsReader, bool durable): 
    scSmplQueueManagerTaskDurable(allowSenderAsReader, durable)
    {};
  virtual ~scSmplQueueManagerTaskRoundRobin() {};
  virtual bool handleReaderResponse(
    scSmplQueueReaderTask &reader, const scString &readerTarget, 
    const scEnvelope &envelope, const scResponse &response);
  virtual bool hasMessageForReader(scSmplQueueReaderTask *reader);
  virtual void handleEnvelopeAccepted(scSmplQueueReaderTask *reader, const scEnvelope &envelope);
private:
  scString m_lastAcceptedReader;
};

/// sends each message to first reader 
class scSmplQueueManagerTaskHighAvail: public scSmplQueueManagerTaskDurable {
public:
  scSmplQueueManagerTaskHighAvail(bool allowSenderAsReader, bool durable): scSmplQueueManagerTaskDurable(allowSenderAsReader, durable) {};
  virtual ~scSmplQueueManagerTaskHighAvail() {};
  //virtual bool get(scEnvelope &a_envelope);
  //virtual int handleMessage(scEnvelope &envelope, scResponse &response);
  //virtual bool needsRun();
  //virtual int intRun();
  virtual bool hasMessageForReader(scSmplQueueReaderTask *reader);
};

/// forwards messages to a selected target address
class scSmplQueueManagerTaskForward: public scSmplQueueManagerTask {
public:
  scSmplQueueManagerTaskForward(bool allowSenderAsReader): 
    scSmplQueueManagerTask(allowSenderAsReader) {}
  virtual ~scSmplQueueManagerTaskForward() {}
};

struct scSmplQueueKeepAliveJobItem {
  scSmplQueueKeepAliveJobItem(const scString &address, const scString &queueName);
  
  void setMessageLimit(uint value);
  void setDelay(uint value);
  void setErrorLimit(uint value);
  void setErrorDelay(uint value);  
  void setRetryListen(bool value);
  void setLastRequestId(int value);
  void clearLastRequestId();
  void setTargetAddress(const scString &value);
  
  uint getMessageLimit();
  uint getDelay();
  uint getErrorLimit();
  uint getErrorDelay();
  bool getRetryListen();
  int getLastRequestId();
  scString getTargetAddress() const;
  
  bool isErrorStatus();
  void checkTimeOut();

  const scString &getAddress();
  const scString &getQueueName();
  
  void markLastContactTime();
  void clearLastContactTime();
  cpu_ticks getTimeLeft();
  void handleErrorArrived();
  void handleSuccessArrived();  
  void handleMessageSent();
  
  uint getErrorCount();
  void incErrorCount();

  uint getMessageCount();
  void incMessageCount();

  bool isValid();  
  bool isWaiting();
  bool needsResult();
protected:    
  uint m_messageLimit;
  uint m_delay;
  uint m_errorLimit;
  uint m_errorDelay;
  int m_lastRequestId;
  bool m_retryListen;
  bool m_errorStatus;
  scString m_address;
  scString m_queueName;
  scString m_targetAddress;
  //---
  cpu_ticks m_lastContactTime;
  uint m_errorCount;
  uint m_messageCount;
};

typedef boost::ptr_list<scSmplQueueKeepAliveJobItem> scSmplQueueKeepAliveJobList;

class scSmplQueueKeepAliveTask: public scTask {
public:
  scSmplQueueKeepAliveTask(scSmplQueueModule *parentModule);
  virtual ~scSmplQueueKeepAliveTask();
  void addJob(const scString &address, const scString &queueName, uint messageLimit, uint delay, uint errorLimit, uint errorDelay, bool retryListen, const scString &targetAddr);
  virtual int handleResponse(scMessage *message, scResponse &response);
  void clrParentModule(scSmplQueueModule *parentModule);
protected:  
  virtual int intRun();
  cpu_ticks calcNextDelay();
  uint processJobs();
  void processJobItem(scSmplQueueKeepAliveJobItem &item);
  void postMessage(const scString &address, const scString &command, const scDataNode *params, int requestId);
  void handleResponseForItem(const scMessage &message, const scResponse &response, scSmplQueueKeepAliveJobItem &item);
  void processItemError(scSmplQueueKeepAliveJobItem &item);
  void processItemSendMarkAlive(scSmplQueueKeepAliveJobItem &item);
  void processItemSendListen(scSmplQueueKeepAliveJobItem &item);
protected:  
  scSmplQueueKeepAliveJobList m_jobs;
  scSmplQueueModule *m_parentModule;
};

class scSmplQueueKeepAliveHandler: public scRequestHandler {
public: 
  scSmplQueueKeepAliveHandler(scSmplQueueKeepAliveTask *owner): m_owner(owner), scRequestHandler() {}
  virtual ~scSmplQueueKeepAliveHandler() {}
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response) {
    m_owner->handleResponse(const_cast<scMessage *>(&a_message), const_cast<scResponse &>(a_response));
  }    
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response) {
    m_owner->handleResponse(const_cast<scMessage *>(&a_message), const_cast<scResponse &>(a_response));
  }         
protected:
  scSmplQueueKeepAliveTask *m_owner;  
};

// ----------------------------------------------------------------------------
// Local class implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scSmplQueueManagerTaskNullDev
// ----------------------------------------------------------------------------
int scSmplQueueManagerTaskNullDev::handleMessage(scEnvelope &envelope, scResponse &response)
{
  return SC_MSG_STATUS_OK;
}

// ----------------------------------------------------------------------------
// scSmplQueueManagerTaskPull
// ----------------------------------------------------------------------------
void scSmplQueueManagerTaskPull::addReader(scSmplQueueReaderTask *reader)
{
  throw scError("Wrong type of queue for readers!");
}

bool scSmplQueueManagerTaskPull::hasMessageForReader(scSmplQueueReaderTask *reader)
{
  // messages are only read using squeue.fetch command
  return false;
}

// ----------------------------------------------------------------------------
// scSmplQueueManagerTaskMultiCast
// ----------------------------------------------------------------------------
bool scSmplQueueManagerTaskMultiCast::get(scEnvelope &a_envelope)
{
  return false;
}

int scSmplQueueManagerTaskMultiCast::handleMessage(scEnvelope &envelope, scResponse &response)
{
  int res;
//  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug("[SQueue] received message: ["+message->getCommand()+"] from: ["+envelope.getSender().getAsString()+"]");  
#endif  
  if (m_limit && (m_waiting.size() >= size_t(m_limit))) 
  {
    res = SC_MSG_STATUS_OVERFLOW;
  } else {  
     put(envelope);    
     res = SC_MSG_STATUS_FORWARDED;
  }  
  
  return res;
}

bool scSmplQueueManagerTaskMultiCast::hasMessageForReader(scSmplQueueReaderTask *reader)
{
  return !isEmpty();
}

bool scSmplQueueManagerTaskMultiCast::needsRun()
{
  return (!m_waiting.empty()) && (!m_readers.empty());
}  

int scSmplQueueManagerTaskMultiCast::intRun()
{
  int res = 0;
  scSmplQueueReaderTask *task;

  scEnvelope envelope;
  while(scSmplQueueManagerTask::get(envelope))
  {
    for (scReaderListIterator p = m_readers.begin(); p != m_readers.end(); p++ )
    {
      task = dynamic_cast<scSmplQueueReaderTask *>(*p);
      if (task->acceptEnvelope(envelope))
        task->forwardEnvelope(envelope);
    }
    res++;
  }

  return res;
}

// ----------------------------------------------------------------------------
// scSmplQueueManagerTaskHighAvail
// ----------------------------------------------------------------------------
/*
bool scSmplQueueManagerTaskHighAvail::get(scEnvelope &a_envelope)
{
  return false;
}

int scSmplQueueManagerTaskHighAvail::handleMessage(scEnvelope &envelope, scResponse &response)
{
  int res;
#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug("[SQueue] received message: ["+message->getCommand()+"] from: ["+envelope.getSender().getAsString()+"]");  
#endif  
  if (m_limit && (m_waiting.size() >= size_t(m_limit))) 
  {
    res = SC_MSG_STATUS_OVERFLOW;
  } else {  
     put(envelope);    
     res = SC_MSG_STATUS_FORWARDED;
  }  
  
  return res;
}
*/

bool scSmplQueueManagerTaskHighAvail::hasMessageForReader(scSmplQueueReaderTask *reader)
{
  scSmplQueueReaderTask *task;
  scReaderListIterator p = m_readers.begin(); 
  task = dynamic_cast<scSmplQueueReaderTask *>(*p);
  return (reader == task) && !isEmpty();
}

/*
bool scSmplQueueManagerTaskHighAvail::needsRun()
{
  return (!m_waiting.empty()) && (!m_readers.empty());
}  

int scSmplQueueManagerTaskHighAvail::intRun()
{
  int res = 0;
  scSmplQueueReaderTask *task;

  scEnvelope envelope;

  if (!m_readers.empty())
  while(scSmplQueueManagerTask::get(envelope))
  {
    scReaderListIterator p = m_readers.begin(); 
    task = dynamic_cast<scSmplQueueReaderTask *>(*p);
    if (task->acceptEnvelope(envelope))
      task->forwardEnvelope(envelope);
    else
      break;
    res++;
  }

  return res;
}
*/

// ----------------------------------------------------------------------------
// scDurableRequestInfo
// ----------------------------------------------------------------------------
scDurableRequestInfo::scDurableRequestInfo()
{
  init();
}

scDurableRequestInfo::scDurableRequestInfo(const scDurableRequestInfo& rhs)
{
  this->m_retryCount = rhs.m_retryCount;
  this->m_startTime = rhs.m_startTime;
  this->m_initTime = rhs.m_initTime;
}

scDurableRequestInfo& scDurableRequestInfo::operator=(const scDurableRequestInfo& rhs)
{
  if (&rhs != this)
  {
    this->m_retryCount = rhs.m_retryCount;
    this->m_startTime = rhs.m_startTime;
    this->m_initTime = rhs.m_initTime;
  }
  
  return *this;
}

scDurableRequestInfo::~scDurableRequestInfo()
{
}

cpu_ticks scDurableRequestInfo::getStartTime() const
{
  return m_startTime;
}

uint scDurableRequestInfo::getRetryCount()
{
  return m_retryCount;
}  

void scDurableRequestInfo::incRetryCount()
{
  m_retryCount++;
}

void scDurableRequestInfo::resetStartTime()
{
   m_startTime = cpu_time_ms();
}

void scDurableRequestInfo::setStartTime(cpu_ticks ticks)
{
  m_startTime = ticks;
}

bool scDurableRequestInfo::isTimeToStart()
{
  cpu_ticks nowTicks = cpu_time_ms();
  return (nowTicks >= m_startTime) || (m_startTime == 0);
}

void scDurableRequestInfo::setReaderName(const scString &name)
{
  m_readerName = name;
}

const scString &scDurableRequestInfo::getReaderName()
{
  return m_readerName;
}

void scDurableRequestInfo::init()
{
  m_startTime = 0;
  m_retryCount = 0;
  m_initTime = cpu_time_ms();
}

// ----------------------------------------------------------------------------
// scSmplQueueManagerTaskDurable
// ----------------------------------------------------------------------------
void scSmplQueueManagerTaskDurable::setRetryLimit(uint value)
{
  m_retryLimit = value;
}

void scSmplQueueManagerTaskDurable::setRetryDelay(cpu_ticks value)
{
  m_retryDelay = value;
}

cpu_ticks scSmplQueueManagerTaskDurable::getRetryDelay()
{
  return m_retryDelay;
}

void scSmplQueueManagerTaskDurable::setContactTimeout(cpu_ticks value)
{
  m_contactTimeout = value;
}

void scSmplQueueManagerTaskDurable::setResultTimeout(cpu_ticks value)
{
  m_resultTimeout = value;
}

void scSmplQueueManagerTaskDurable::setStoreTimeout(cpu_ticks value)
{
  m_storeTimeout = value;
}

bool scSmplQueueManagerTaskDurable::handleReaderResponse(
  scSmplQueueReaderTask &reader, const scString &readerTarget, 
  const scEnvelope &envelope, const scResponse &response
)
{
  uint reqId = envelope.getEvent()->getRequestId();  

#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug(
    scString("[SQueue] Response arrived: ")+toString(reqId)+
     ", command: "+dynamic_cast<scMessage *>(envelope.getEvent())->getCommand()+
     ", error: "+(response.isError()?"true":"false")
  );
#endif

  if (!m_durable || !response.isError()) {
    clearRequestInfo(reqId);
    return true;
  }  
    
  return handleRequestError(reqId, envelope, response);
}

//bool scSmplQueueManagerTaskDurable::handleRequestError(uint reqId, const scResponse &response)
//{
//  scEnvelopeColn::iterator it = m_waiting.begin();
//  while((it != m_waiting.end()) && (static_cast<uint>(it->getEvent()->getRequestId()) != reqId))
//  {
//    it++;
//  }  
//
//  if (it == m_waiting.end())
//    return false;
//
//  scEnvelope envelope = *it;
//  
//  Log::addDebug(scString("SQMTRR01: Removing request from queue: ")+toString(reqId));
//  m_waiting.erase(it);
//  
//  handleRequestError(reqId, envelope, response);
//}

bool scSmplQueueManagerTaskDurable::handleRequestError(uint reqId, const scEnvelope &envelope, const scResponse &response)
{
  uint retryCount = getRetryCount(reqId);

  if ((retryCount >= m_retryLimit) && (m_retryLimit > 0))
  {
    scResponse newResponse(response);
    newResponse.setRequestId(reqId);
    sendRequestFailed(reqId, newResponse);
    clearRequestInfo(reqId);
    return false;
  }
    
  prepareRetry(reqId, envelope);
  return false;
}  

// set reader for request
void scSmplQueueManagerTaskDurable::handleEnvelopeSent(scSmplQueueReaderTask *reader, const scEnvelope &envelope)
{
  uint reqId = envelope.getEvent()->getRequestId();  
  scDurableRequestInfoMap::iterator it = m_requestMap.find(reqId);
  if (it != m_requestMap.end())
  {
    (*it)->second->setReaderName(reader->getName());
  }  
}

//- increase retry count
//- set timestamp when envelope will be ready for use
//- add message to waiting 
void scSmplQueueManagerTaskDurable::prepareRetry(uint reqId, const scEnvelope &envelope)
{
  scDurableRequestInfoMap::iterator it = m_requestMap.find(reqId);
  if (it != m_requestMap.end())
  {
    (*it)->second->incRetryCount();
    (*it)->second->setStartTime(cpu_time_ms() + getRetryDelay());
    putRetry(envelope);
  } else {
    Log::addError(scString("RR: Uknown request received: ")+toString(reqId));
  }
}

uint scSmplQueueManagerTaskDurable::getRetryCount(uint reqId)
{
  uint res = 0;
  scDurableRequestInfoMap::iterator it = m_requestMap.find(reqId);
  if (it != m_requestMap.end())
    res = (*it)->second->getRetryCount();
  return res;  
}

// remove request info from queue
void scSmplQueueManagerTaskDurable::clearRequestInfo(uint reqId)
{
  scDurableRequestInfoMap::iterator it = m_requestMap.find(reqId);
#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug(scString("[SQueue] Removing request from map: ")+toString(reqId));
#endif
  if (it != m_requestMap.end())
    m_requestMap.erase(it);
  eraseFromWaiting(reqId);  
}

bool scSmplQueueManagerTaskDurable::sendRequestFailed(uint reqId,     
  int statusCode) 
{                     
  scResponse response;   
  response.setRequestId(reqId);
  response.setStatus(statusCode);
  return sendRequestFailed(reqId, response);  
}
            
bool scSmplQueueManagerTaskDurable::sendRequestFailed(uint reqId,     
  const scResponse &response)
{
 
  scEnvelopeColn::iterator it = m_waiting.begin();
  while((it != m_waiting.end()) && (static_cast<uint>(it->getEvent()->getRequestId()) != reqId))
  {
    it++;
  }  

  if (it == m_waiting.end())
    return false;
  
  scMessageAddress ownAddr(getScheduler()->getOwnAddress(it->getSender().getProtocol()));
  
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope(ownAddr, it->getSender(), new scResponse(response)));
 //copy requestId from original message
  envelopeGuard->getEvent()->setRequestId(it->getEvent()->getRequestId());
 //post response to original sender
  getScheduler()->postEnvelope(envelopeGuard.release());
  
  return true;
}

// read next message ready to be used (use request info)
bool scSmplQueueManagerTaskDurable::get(scEnvelope &a_envelope)
{
  bool res = false;
  uint reqId;

  scEnvelopeColn::iterator it = m_waiting.begin();
  scDurableRequestInfoMap::iterator itr;
  
  while(!res && (it != m_waiting.end()))
  {
    reqId = it->getEvent()->getRequestId();
    itr = m_requestMap.find(reqId);
    assert(reqId != SC_REQUEST_ID_NULL);
    if (itr == m_requestMap.end())
      throw scError(scString("Unknown request found"))
         .addDetails("request_id", scDataNode(reqId))
         .addDetails("queue", scDataNode(getName()));
         
    assert(itr != m_requestMap.end());
    if ((*itr)->second->isTimeToStart())
    {
      (*itr)->second->resetStartTime();
      a_envelope = *it;
#ifdef SMPL_QUEUE_LOG_ENABLED
      Log::addDebug(scString("[SQueue] Removing request from queue: ")+toString(reqId));
#endif
      m_waiting.erase(it);
      res = true;
    } else {
      ++it;
    }    
  }
      
  return res;  
}

void scSmplQueueManagerTaskDurable::eraseFromWaiting(uint reqId)
{
  scEnvelopeColn::iterator it = m_waiting.begin();
  
  while((it != m_waiting.end()) && (static_cast<uint>(it->getEvent()->getRequestId()) != reqId))
  {
    it++;
  }  
  
  if (it != m_waiting.end())
    m_waiting.erase(it);    
}

// add message to waiting queue
// generate request info
void scSmplQueueManagerTaskDurable::put(const scEnvelope &envelope)
{
  intPut(envelope, false);
}

void scSmplQueueManagerTaskDurable::putRetry(const scEnvelope &envelope)
{
  intPut(envelope, true);
}

void scSmplQueueManagerTaskDurable::intPut(const scEnvelope &envelope, bool retry)
{
  uint reqId = envelope.getEvent()->getRequestId();
  
#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug(scString("[SQueue] Adding request to map: ")+toString(reqId));
#endif
  
  scDurableRequestInfoMap::iterator it = m_requestMap.find(reqId);
  if (it == m_requestMap.end())
  {
    m_requestMap.insert(reqId, new scDurableRequestInfo());  
    it = m_requestMap.find(reqId);
  } else if (retry) {
    it->second->setReaderName("");
  } else {
    throw scError("Request already in queue")
         .addDetails("request_id", scDataNode(reqId))
         .addDetails("queue", scDataNode(getName()));
  }  

  (*it)->second->resetStartTime();
  scSmplQueueManagerTask::put(envelope);  

#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug(scString("[SQueue] Adding request to queue: ")+toString(reqId));
#endif
}

int scSmplQueueManagerTaskDurable::intRun()
{
  int res = scSmplQueueManagerTask::intRun();
  validateReaders();
  validateRequests();
  sleepFor(DEF_QUEUE_VALIDATE_DELAY);
  return res;
}

void scSmplQueueManagerTaskDurable::validateReaders()
{
  if (m_contactTimeout == 0)
    return;
    
  cpu_ticks lastContact;

  // validate last contact
  if (m_contactTimeout > 0) 
  for (scReaderListIterator p = m_readers.begin(); p != m_readers.end(); /* nothing here */ )
  {
    lastContact = dynamic_cast<scSmplQueueReaderTask *>(*p)->getLastContactTime();    
    if (is_cpu_time_elapsed_ms(lastContact, m_contactTimeout))
    {
      Log::addWarning(scString("Reader contact timeout, queue: ")+getName()+", reader: "+dynamic_cast<scSmplQueueReaderTask *>(*p)->getTarget());
      dynamic_cast<scSmplQueueReaderTask *>(*p)->cancelAll();
      (*p)->requestStop();
    }
    ++p;
  }
}

void scSmplQueueManagerTaskDurable::validateRequests()
{
  scDurableRequestInfoMap::iterator itr = m_requestMap.begin();
  scSmplQueueReaderTask *reader;
  typedef std::map<uint, scString> cancelMap;
  
  cancelMap requestForCancel;  
  scString rname;
  
  while(itr != m_requestMap.end())
  {
    rname = (*itr).second->getReaderName();
    if ((!rname.empty()) && (m_resultTimeout > 0) && (is_cpu_time_elapsed_ms((*itr).second->getStartTime(), m_resultTimeout)))
      requestForCancel.insert(std::make_pair((*itr).first, rname));
    else if ((rname.empty()) && (m_storeTimeout > 0) && (is_cpu_time_elapsed_ms((*itr).second->getStartTime(), m_storeTimeout)))
      requestForCancel.insert(std::make_pair((*itr).first, rname));
    else if ((!rname.empty()) && (findReader((*itr).second->getReaderName()) == m_readers.end())) {
      requestForCancel.insert(std::make_pair((*itr).first, rname));
    }  
    ++itr;
  }
    
  cancelMap::iterator cancIt;
  cancIt = requestForCancel.begin();
  
  while(cancIt != requestForCancel.end())
  {    
    if (!(*cancIt).second.empty())      
      reader = dynamic_cast<scSmplQueueReaderTask *>(*(findReader((*cancIt).second)));
    else
      reader = SC_NULL;
        
    if (reader != SC_NULL)
    {
    // outdated & sent, reader found
      Log::addWarning(scString("Result timeout, queue: ")+getName()+", request-id: "+toString((*cancIt).first));
      reader->cancelRequest((*cancIt).first, SC_RESP_STATUS_TIMEOUT);
      clearRequestInfo((*cancIt).first);
    } else if ((*cancIt).second.empty())
    { 
    // outdated & not sent yet
      Log::addWarning(scString("Request outdated - removing, queue: ")+getName()+", request-id: "+toString((*cancIt).first));
      sendRequestFailed((*cancIt).first, SC_RESP_STATUS_TIMEOUT);
      clearRequestInfo((*cancIt).first);
    } else {
    // outdated & sent, but to unknown reader
      Log::addWarning(scString("Reader not found - removing request, queue: ")+getName()+", request-id: "+toString((*cancIt).first));
      sendRequestFailed((*cancIt).first, SC_RESP_STATUS_TIMEOUT);
      clearRequestInfo((*cancIt).first);
    }
    ++cancIt;
  }  
}

// ----------------------------------------------------------------------------
// scSmplQueueManagerTaskRoundRobin
// ----------------------------------------------------------------------------
bool scSmplQueueManagerTaskRoundRobin::handleReaderResponse(
  scSmplQueueReaderTask &reader, const scString &readerTarget, 
  const scEnvelope &envelope, const scResponse &response
)
{
  if (m_lastAcceptedReader == reader.getName())
    m_lastAcceptedReader = "";
  return inherited::handleReaderResponse(reader, readerTarget, envelope, response);
}

bool scSmplQueueManagerTaskRoundRobin::hasMessageForReader(scSmplQueueReaderTask *reader)
{
  if (isEmpty())
    return false;

  if (m_readers.size() <= 1)
    return true;

  // do not send 2 messages to the same reader in row
  bool res = (reader->getName() != m_lastAcceptedReader);

  return res;
}

void scSmplQueueManagerTaskRoundRobin::handleEnvelopeAccepted(scSmplQueueReaderTask *reader, const scEnvelope &envelope)
{
  m_lastAcceptedReader = reader->getName();
  inherited::handleEnvelopeAccepted(reader, envelope);
}

// ----------------------------------------------------------------------------
// scSmplQueueKeepAliveTask
// ----------------------------------------------------------------------------
scSmplQueueKeepAliveTask::scSmplQueueKeepAliveTask(scSmplQueueModule *parentModule): scTask(), 
  m_parentModule(parentModule)
{
}

scSmplQueueKeepAliveTask::~scSmplQueueKeepAliveTask()
{
  if (m_parentModule != SC_NULL)
    m_parentModule->clearTask(this);
}

void scSmplQueueKeepAliveTask::clrParentModule(scSmplQueueModule *parentModule)
{
  if (m_parentModule == parentModule)
    m_parentModule = SC_NULL;
}

void scSmplQueueKeepAliveTask::addJob(const scString &address, const scString &queueName, 
  uint messageLimit, uint delay, uint errorLimit, uint errorDelay, bool retryListen, const scString &targetAddr)
{
  std::auto_ptr<scSmplQueueKeepAliveJobItem> itemGuard(new scSmplQueueKeepAliveJobItem(address, queueName));
  itemGuard->setDelay(delay); 
  itemGuard->setMessageLimit(messageLimit);
  itemGuard->setErrorLimit(errorLimit);
  itemGuard->setErrorDelay(errorDelay);
  itemGuard->setRetryListen(retryListen);
  itemGuard->setTargetAddress(targetAddr);
  m_jobs.push_back(itemGuard.release());
}

int scSmplQueueKeepAliveTask::intRun()
{ 
  uint res = processJobs();
  sleepFor(calcNextDelay());
  return res;
}

cpu_ticks scSmplQueueKeepAliveTask::calcNextDelay()
{
  const cpu_ticks DEF_RUN_DELAY = 50;
  cpu_ticks res = DEF_RUN_DELAY; 
  bool first = true;
  scSmplQueueKeepAliveJobList::iterator it;

  for(it = m_jobs.begin(); it != m_jobs.end(); ++it)
  {
    if (first)
      res = it->getTimeLeft();
    else
      res = SC_MIN(it->getTimeLeft(), res);  
  }
  
  return res;  
}

// find jobs for next contact & send messages
uint scSmplQueueKeepAliveTask::processJobs()
{
  uint res = 0;
  scSmplQueueKeepAliveJobList::iterator it;
  for(it = m_jobs.begin(); it != m_jobs.end(); /* empty here */)
  {
    processJobItem(*it);
    res++;
    if (!it->isValid())
      it = m_jobs.erase(it);
    else
      ++it;
  }
  return res;
}

void scSmplQueueKeepAliveTask::processJobItem(scSmplQueueKeepAliveJobItem &item)
{
  uint msgLimit = item.getMessageLimit();
  if (!item.isWaiting())
  if (item.isValid())
  if ((msgLimit == 0) || (item.getMessageCount() < msgLimit))  
  if (item.getTimeLeft() == 0)
  {
    item.checkTimeOut();
    if (item.isErrorStatus())
      processItemError(item);
    else
      processItemSendMarkAlive(item);
  }
}

void scSmplQueueKeepAliveTask::processItemError(scSmplQueueKeepAliveJobItem &item)
{
  if (!item.isValid())
    return;
    
  if (item.getRetryListen())
  {
    Log::addDebug("[SQueueKeepAlive] Queue contact timeout - sending listen...");
    item.clearLastRequestId();
    processItemSendListen(item);
  }   
}

void scSmplQueueKeepAliveTask::processItemSendMarkAlive(scSmplQueueKeepAliveJobItem &item)
{
  scDataNode params;
  params.addChild("queue_name", new scDataNode(item.getQueueName()));
  scString fullAddr = item.getAddress();
  fullAddr = getScheduler()->evaluateAddress(fullAddr);
  scMessageAddress ownAdrStruct(getOwnAddress(scMessageAddress(fullAddr).getProtocol()));
  // clear task
  ownAdrStruct.setTask("");    
  scString ownAddress = ownAdrStruct.getAsString();
  params.addChild("source_name", new scDataNode(ownAddress));

  Log::addDebug("[SQueueKeepAlive] Sending mark_alive...");
  
  if (item.needsResult()) {
    int reqId = getNextRequestId();
    item.setLastRequestId(reqId);
    this->postMessage(item.getAddress(), "squeue.mark_alive", &params, reqId);
  } else { 
    getScheduler()->postMessage(item.getAddress(), "squeue.mark_alive", &params);
  }  

  item.handleMessageSent();
}

void scSmplQueueKeepAliveTask::processItemSendListen(scSmplQueueKeepAliveJobItem &item)
{
  scDataNode params;
  params.addChild("queue_name", new scDataNode(item.getQueueName()));
  params.addChild("exec_at_addr", new scDataNode(item.getAddress()));
  scString targetAddr = item.getTargetAddress();
  if (!targetAddr.empty())
    params.addChild("target_addr", new scDataNode(targetAddr));

  getScheduler()->postMessage(getScheduler()->getOwnAddress().getAsString(), "squeue.listen_at", &params);
  item.handleMessageSent();
}

void scSmplQueueKeepAliveTask::postMessage(const scString &address, const scString &command, const scDataNode *params, int requestId)
{
   std::auto_ptr<scEnvelope> envelopeGuard(
     new scEnvelope(
       scMessageAddress(getOwnAddress(scMessageAddress(address).getProtocol())), 
       scMessageAddress(address), 
       new scMessage(command, params, requestId)));
       
   //post 
   getScheduler()->postEnvelope(envelopeGuard.release(), new scSmplQueueKeepAliveHandler(this));        
}

int scSmplQueueKeepAliveTask::handleResponse(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  
  if (response.getRequestId() != SC_REQUEST_ID_NULL)
  {  
    int requestId = response.getRequestId();
    scSmplQueueKeepAliveJobList::iterator it;
    for(it = m_jobs.begin(); it != m_jobs.end(); ++it)
    {
      if (it->getLastRequestId() == requestId)
      {
        handleResponseForItem(*message, response, *it);
        res = SC_MSG_STATUS_OK;
        break;
      }
    }    
  }
  return res;
}

void scSmplQueueKeepAliveTask::handleResponseForItem(const scMessage &message, const scResponse &response, scSmplQueueKeepAliveJobItem &item)
{
  if (response.isError())
    item.handleErrorArrived();
  else
    item.handleSuccessArrived();  
}

// ----------------------------------------------------------------------------
// scSmplQueueKeepAliveJobItem
// ----------------------------------------------------------------------------
scSmplQueueKeepAliveJobItem::scSmplQueueKeepAliveJobItem(const scString &address, const scString &queueName):
  m_address(address), m_queueName(queueName), 
  m_messageLimit(1), m_delay(500), m_errorLimit(0), 
  m_errorCount(0), m_messageCount(0), m_errorDelay(0), m_retryListen(false), m_lastRequestId(SC_REQUEST_ID_NULL),
  m_errorStatus(false)
{
  //markLastContactTime();  
  clearLastContactTime(); // force send "mark_alive" immediately after start  
}
  
void scSmplQueueKeepAliveJobItem::setMessageLimit(uint value)
{
  m_messageLimit = value;
}

void scSmplQueueKeepAliveJobItem::setDelay(uint value)
{
  m_delay = value;
}

void scSmplQueueKeepAliveJobItem::setErrorLimit(uint value)
{
  m_errorLimit = value;
}
  
void scSmplQueueKeepAliveJobItem::setErrorDelay(uint value)
{
  m_errorDelay = value;
}

void scSmplQueueKeepAliveJobItem::setRetryListen(bool value)
{
  m_retryListen = value;
}

void scSmplQueueKeepAliveJobItem::setLastRequestId(int value)
{
  m_lastRequestId = value;
}

void scSmplQueueKeepAliveJobItem::clearLastRequestId()
{
  m_lastRequestId = SC_REQUEST_ID_NULL;
}
  
void scSmplQueueKeepAliveJobItem::setTargetAddress(const scString &value)
{
  m_targetAddress = value;
}  
  
uint scSmplQueueKeepAliveJobItem::getMessageLimit()
{
  return m_messageLimit;
}

uint scSmplQueueKeepAliveJobItem::getDelay()
{
  return m_delay;
}

uint scSmplQueueKeepAliveJobItem::getErrorLimit()
{
  return m_errorLimit;
}

uint scSmplQueueKeepAliveJobItem::getErrorDelay()
{
  return m_errorDelay;
}

bool scSmplQueueKeepAliveJobItem::getRetryListen()
{
  return m_retryListen;
}

int scSmplQueueKeepAliveJobItem::getLastRequestId()
{
  return m_lastRequestId;
}
 
const scString &scSmplQueueKeepAliveJobItem::getAddress()
{
  return m_address;
}

const scString &scSmplQueueKeepAliveJobItem::getQueueName()
{
  return m_queueName;
}  

scString scSmplQueueKeepAliveJobItem::getTargetAddress() const
{
  return m_targetAddress;
}
  
void scSmplQueueKeepAliveJobItem::markLastContactTime()
{
  m_lastContactTime = cpu_time_ms();
}

void scSmplQueueKeepAliveJobItem::clearLastContactTime()
{
  m_lastContactTime = 0;
}

cpu_ticks scSmplQueueKeepAliveJobItem::getTimeLeft()
{
  cpu_ticks nowTime = cpu_time_ms();
  cpu_ticks delay = calc_cpu_time_delay(m_lastContactTime, nowTime);
  if (m_lastContactTime > nowTime)
    return 1; // not yet 
  else if (delay >= m_delay)
    return 0;
  else
    return m_delay - delay;  
}
  
void scSmplQueueKeepAliveJobItem::handleErrorArrived()
{
  incErrorCount();
  markLastContactTime();  
  if (m_errorDelay > 0)
    m_lastContactTime += m_errorDelay;
  m_errorStatus = true;
}

void scSmplQueueKeepAliveJobItem::handleSuccessArrived()
{
  markLastContactTime();  
  m_errorStatus = false;
  m_lastRequestId = SC_REQUEST_ID_NULL;
}

void scSmplQueueKeepAliveJobItem::handleMessageSent()
{
  incMessageCount();
  markLastContactTime();  
  m_errorStatus = false;
}
    
bool scSmplQueueKeepAliveJobItem::isErrorStatus()
{  
  return m_errorStatus;
}

void scSmplQueueKeepAliveJobItem::checkTimeOut()
{  
  if (!m_errorStatus && (m_lastRequestId != SC_REQUEST_ID_NULL) && (getTimeLeft() == 0))
  {
    m_errorStatus = true;
    m_lastRequestId = SC_REQUEST_ID_NULL;
  }
}

uint scSmplQueueKeepAliveJobItem::getErrorCount()
{
  return m_errorCount;
}

void scSmplQueueKeepAliveJobItem::incErrorCount()
{
  m_errorCount++;
}

uint scSmplQueueKeepAliveJobItem::getMessageCount()
{
  return m_messageCount;
}

void scSmplQueueKeepAliveJobItem::incMessageCount()
{
  m_messageCount++;
}

bool scSmplQueueKeepAliveJobItem::isValid()
{
  bool res = true;
  if ((m_messageLimit > 0) && (m_messageCount >= m_messageLimit))
    res = false;
  else if ((m_errorLimit > 0) && (m_errorCount >= m_errorLimit))
    res = false;
  return res;  
}

bool scSmplQueueKeepAliveJobItem::isWaiting()
{
  return (m_lastRequestId != SC_REQUEST_ID_NULL) && (getTimeLeft() > 0);
}

bool scSmplQueueKeepAliveJobItem::needsResult()
{
  return (m_errorLimit > 0) || (m_retryListen);
}

// ----------------------------------------------------------------------------
// Public class implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scSmplQueueManagerTask
// ----------------------------------------------------------------------------
scSmplQueueManagerTask::scSmplQueueManagerTask(bool allowSenderAsReader):
  scTask()
{
  m_limit = 0;
  //m_lastReaderName = "";
  m_allowSenderAsReader = allowSenderAsReader;
}

scSmplQueueManagerTask::~scSmplQueueManagerTask()
{
  disconnectReaders();
}

int scSmplQueueManagerTask::handleMessage(scEnvelope &envelope, scResponse &response)
{
  int res;
  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
#ifdef SMPL_QUEUE_LOG_ENABLED
  Log::addDebug("[SQueue] received message: ["+message->getCommand()+"] from: ["+envelope.getSender().getAsString()+"]");  
#endif  
  if (m_limit && (m_waiting.size() >= size_t(m_limit))) 
  {
    res = SC_MSG_STATUS_OVERFLOW;
  } else if (message->getRequestId() == SC_REQUEST_ID_NULL) {
    res = SC_MSG_STATUS_MSG_ID_REQ;
  } else {  
     put(envelope);    
     res = SC_MSG_STATUS_FORWARDED;
  }  
  
  return res;
}

void scSmplQueueManagerTask::addReader(scSmplQueueReaderTask *reader)
{
  m_readers.push_back(reader);
}

void scSmplQueueManagerTask::removeReader(scSmplQueueReaderTask *reader)
{
  m_readers.remove(reader);   
}

void scSmplQueueManagerTask::deleteReaders()
{
  for (scReaderListIterator p = m_readers.begin(); p != m_readers.end(); /* nothing here */ )
  {
    getScheduler()->deleteTask(*p);
    //delete(*p);
    p = m_readers.erase(p);  
  }
}

void scSmplQueueManagerTask::disconnectReaders()
{
  while(m_readers.begin() != m_readers.end())
    dynamic_cast<scSmplQueueReaderTask *>(*m_readers.begin())->setQueueManager(SC_NULL);
}

bool scSmplQueueManagerTask::get(scEnvelope &a_envelope)
{
  bool res = false;
    
  if (!m_waiting.empty()) { 
    //scEnvelopeTransport transp = m_waiting.pop_back();
    scEnvelopeTransport transp = m_waiting.pop_front();
    a_envelope = *transp;   
    res = true;
  }
  
  return res;
}

void scSmplQueueManagerTask::clearQueue()
{
  m_waiting.clear();
}

void scSmplQueueManagerTask::setLimit(int value)
{  
  m_limit = value;
}

int scSmplQueueManagerTask::getLimit() const
{
  return m_limit;
}

bool scSmplQueueManagerTask::isEmpty() const
{
  return (m_waiting.size() <= 0);
}

void scSmplQueueManagerTask::put(const scEnvelope &envelope)
{
   m_waiting.push_back(new scEnvelope(envelope));
}

scString scSmplQueueManagerTask::getStatus()
{
  scString res = "Waiting-messages: "+toString(m_waiting.size())+
    ", readers: "+toString(m_readers.size());
  return res;
}

bool scSmplQueueManagerTask::needsRun()
{
  return false;
}  

void scSmplQueueManagerTask::getReaderList(scStringList &list)
{
  scSmplQueueReaderTask *task;
  
  list.clear();
  for (scReaderListIterator p = m_readers.begin(); p != m_readers.end(); p++ )
  {
    task = dynamic_cast<scSmplQueueReaderTask *>(*p);
    list.push_back(task->getTarget());
  }
}

bool scSmplQueueManagerTask::hasReader(const scString &readerTarget)
{
  bool res = false;
  scSmplQueueReaderTask *task;
  
  for (scReaderListIterator p = m_readers.begin(); p != m_readers.end(); p++ )
  {
    task = dynamic_cast<scSmplQueueReaderTask *>(*p);
    if (task->getTarget() == readerTarget)
    {
      res = true;
      break;
    }   
  }
  return res;
}

//bool scSmplQueueManagerTask::hasMessageForReader(scSmplQueueReaderTask *reader)
//{
//  bool res = false;
//  if (!isEmpty()) 
//  {
//    scString nextName = findNextReaderName();
//    if ((reader->getName() == nextName))
//    {
//      m_lastReaderName = nextName;
//      res = true;
//    }  
//  }  
//  return res;
//}

bool scSmplQueueManagerTask::hasMessageForReader(scSmplQueueReaderTask *reader)
{
  return !isEmpty();
}

scString scSmplQueueManagerTask::findNextReaderName(const scString &readerName)
{
  //scReaderListIterator p = findReader(m_lastReaderName);
  scReaderListIterator p = findReader(readerName);
  if (p != m_readers.end())
  { 
    p++;
  }
  if (p == m_readers.end()) 
  {
    p = m_readers.begin();
  }  
  return ((*p)->getName());
}

scReaderListIterator scSmplQueueManagerTask::findReader(const scString &name)
{
  scReaderListIterator p;
  
  for(p=m_readers.begin(); p!=m_readers.end(); ++p) {
    if ((*p)->getName() == name) 
      break;
  }
  
  return p;
}  

bool scSmplQueueManagerTask::handleReaderResponse(
  scSmplQueueReaderTask &reader, const scString &readerTarget,   
  const scEnvelope &envelope, const scResponse &response
)
{
  return true;
}

void scSmplQueueManagerTask::handleEnvelopeAccepted(scSmplQueueReaderTask *reader, const scEnvelope &envelope)
{
  // empty here
}

void scSmplQueueManagerTask::handleEnvelopeSent(scSmplQueueReaderTask *reader, const scEnvelope &envelope)
{
  // empty here
}

bool scSmplQueueManagerTask::markReaderAlive(const scString &readerAddr)
{
  bool res = false;
  
  scReaderListIterator p;
  scSmplQueueReaderTask *reader;
  
  for(p=m_readers.begin(); p!=m_readers.end(); ++p) {
    reader = dynamic_cast<scSmplQueueReaderTask *>(*p);
    if (reader->getTarget() == readerAddr) 
    {
      reader->noteContactEvent();
      res = true;
    }
  }
  
  return res;
}

// ----------------------------------------------------------------------------
// scSmplQueueReaderTask
// ----------------------------------------------------------------------------

scSmplQueueReaderTask::scSmplQueueReaderTask(): scTask()
{
  m_limit = 1;
  m_lastContactTime = cpu_time_ms();
  m_queueManager = SC_NULL;
}

scSmplQueueReaderTask::~scSmplQueueReaderTask()
{
  setQueueManager(SC_NULL);
  Log::addDebug("[SQueue] Queue reader destroy");
}

void scSmplQueueReaderTask::setQueueManager(scSmplQueueManagerTask *queueManager)
{
  if (m_queueManager != queueManager) 
  {
    if (m_queueManager != SC_NULL)
      m_queueManager->removeReader(this);
    m_queueManager = queueManager;
    if (m_queueManager != SC_NULL) {
      m_queueManager->addReader(this);
      m_allowSenderAsReader = m_queueManager->getAllowSenderAsReader();
    }  
  }
}

scSmplQueueManagerTask *scSmplQueueReaderTask::getQueueManager()
{
  return m_queueManager;
}

void scSmplQueueReaderTask::setTarget(const scString &target)
{
  m_target = target;
}

scString scSmplQueueReaderTask::getTarget() const
{
  return m_target;
}

void scSmplQueueReaderTask::setLimit(int value)
{
  m_limit = value;
}

int scSmplQueueReaderTask::getLimit() const
{
  return m_limit; 
}

void scSmplQueueReaderTask::noteContactEvent()
{
  m_lastContactTime = cpu_time_ms();
}

cpu_ticks scSmplQueueReaderTask::getLastContactTime()
{
  return m_lastContactTime;
}

void scSmplQueueReaderTask::addWaitingMsg(scEnvelope &envelope, int requestId)
{
  //scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());  
  scRequestHandlerTransporter emptyTransporter;
  m_waitingRequests.insert(requestId, 
    new scRequestItem(envelope, emptyTransporter));
}

bool scSmplQueueReaderTask::extractWaitingMsg(int requestId, scRequestItem &foundItem)
{
  bool res;
  scRequestItemMapColnIterator p;
  res = false;
  p = m_waitingRequests.find(requestId);
  if(p != m_waitingRequests.end()) {
      res = true;
      foundItem = *p->second;
      m_waitingRequests.erase(p);
      ++m_processed;
  }      
  return res;
}

int scSmplQueueReaderTask::handleResponse(scMessage *message, scResponse &response)
{
  return handleResponse(message->getRequestId(), response);
}
  
int scSmplQueueReaderTask::handleResponse(uint requestId, scResponse &response)
{
   int res;
   scRequestItem requestItem; 
   if (!extractWaitingMsg(requestId, requestItem))
   {
     res = SC_MSG_STATUS_UNK_MSG;
   } else {
     res = SC_MSG_STATUS_OK;
     noteContactEvent();
     
#ifdef SMPL_QUEUE_TIMERS_ENABLED
     cpu_ticks procTime;
     procTime = calc_cpu_time_delay(requestItem.getStartTime(), cpu_time_ms());
     Timer::inc("msg-proc-squeue-reader", procTime);
     scString cmdTime = dynamic_cast<scMessage *>(requestItem.getEnvelope()->getEvent())->getCommand();
     Timer::inc("msg-proc-squeue-reader-"+cmdTime, procTime);
#endif

#ifdef SMPL_QUEUE_COUNTERS_ENABLED
     Counter::inc("msg-proc-squeue-reader", 1);
     scString cmdCount = dynamic_cast<scMessage *>(requestItem.getEnvelope()->getEvent())->getCommand();
     Counter::inc("msg-proc-squeue-reader-"+cmdCount, 1);
#endif

#ifdef SMPL_QUEUE_LOG_ENABLED
     Log::addDebug("SQueue received response from: ["+m_target+"]");     
#endif  
     bool returnResponse = false;
     
     if (getQueueManager()->handleReaderResponse(*this, m_target, *(requestItem.getEnvelope()), response))
       returnResponse = true;

     if (returnResponse) {
       if (response.isError())
         Log::addDebug(scString("SQueue returns error from: [")+m_target+"] for message: ["+toString(requestId)+"]");     
       sendResponse(*(requestItem.getEnvelope()), response);
     }
   }
   
   return res;
}

void scSmplQueueReaderTask::sendResponse(const scEnvelope &envelope, const scResponse &response)
{
   std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope(scMessageAddress(m_target), envelope.getSender(), new scResponse(response)));
   //copy requestId from original message
   envelopeGuard->getEvent()->setRequestId(envelope.getEvent()->getRequestId());
   //post response to original sender
   getScheduler()->postEnvelope(envelopeGuard.release());
}

bool scSmplQueueReaderTask::needsRun()
{
  //return scTask::needsRun() && (m_waitingRequests.size()>0) && (m_queueManager != SC_NULL) && (!m_queueManager->isEmpty());
  bool res = scTask::needsRun();
  if (res)
    res = isMessageReadyForRead();
  return res;
}

bool scSmplQueueReaderTask::isMessageReadyForRead()
{
  bool res = false;
  if (m_target.length())
    if (isBelowLimit())
      if ((m_queueManager != SC_NULL) && m_queueManager->hasMessageForReader(this))
        res = true;
  return res;
}

int scSmplQueueReaderTask::intRun()
{
  int res = 0;
  bool found;
  if (isMessageReadyForRead())
  {
    do {
      found = false;
      if (isBelowLimit() && !m_queueManager->isEmpty())
      {
        scEnvelope envelope;
        if (m_queueManager->get(envelope))
        {
          if (!acceptEnvelope(envelope)) {
          // give back the message
            m_queueManager->put(envelope);
          } else if (forwardEnvelope(envelope))
          {
            res++;
            found = true;
            m_queueManager->handleEnvelopeSent(this, envelope);
          } else {
            throw scError("Forward message failed");
          }
        } // read OK
      } // messages ready for read
    } while (found);
  } // if  
  return res;
}


int scSmplQueueReaderTask::runStopping()
{
  Log::addDebug("Queue reader - run stopping");
  cancelAll();
  return scTask::runStopping();
}

bool scSmplQueueReaderTask::isBelowLimit()
{
  if (!m_limit || (m_waitingRequests.size() < size_t(m_limit))) 
    return true;
  else
    return false;  
}

bool scSmplQueueReaderTask::acceptEnvelope(const scEnvelope &envelope)
{
  bool res = false;
  if (isBelowLimit() && !envelope.getEvent()->isResponse())
  {
    scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());  
    scDataNode &params = message->getParams(); 
    bool senderOk = m_allowSenderAsReader || (envelope.getSender().getAsString() != m_target);
    if (!senderOk && params.hasChild("_squeue"))
    {
      scDataNode &squeueParams = params["_squeue"];
      if (!squeueParams.getBool("skip_sender", true))
      {
        senderOk = true;
      }  
    }  
    res = senderOk;
  }

  if (res && (m_queueManager != NULL))
    m_queueManager->handleEnvelopeAccepted(this, envelope);

  return res;
}

// send message to reader - remote node
bool scSmplQueueReaderTask::forwardEnvelope(scEnvelope &envelope)
{
  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());  
  bool res = false;

  if ((message != SC_NULL) && (message->getRequestId() != SC_REQUEST_ID_NULL))
  {
    assert(m_target.length()>0);  
    assert(getScheduler() != SC_NULL);  
    scMessageAddress newReceiver(m_target);
    scMessageAddress newSender = getOwnAddress(newReceiver.getProtocol());
    std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope(envelope)); 
    scEnvelope *outEnvelope = envelopeGuard.get();
    outEnvelope->setSender(newSender);
    outEnvelope->setReceiver(m_target);
    int outRequestId = getNextRequestId();
    outEnvelope->getEvent()->setRequestId(outRequestId);

#ifdef SMPL_QUEUE_LOG_ENABLED
     Log::addDebug("SQueue: forwarding envelope to: ["+m_target+"]");     
#endif  
    getScheduler()->postEnvelope(envelopeGuard.release());
    
    addWaitingMsg(envelope, outRequestId);
    res = true;
  }

  return res;
}

void scSmplQueueReaderTask::cancelAll()
{
  scRequestItemMapColnIterator p = m_waitingRequests.begin();
  std::set<uint> idSet;
  std::set<uint>::iterator it;
  
  while (p != m_waitingRequests.end()) {
    idSet.insert((*p).first);
    ++p;
  }
  
  it = idSet.begin();
  while(it != idSet.end())
  {
    cancelRequest(*it, SC_RESP_STATUS_TIMEOUT);
    ++it;
  }
}

void scSmplQueueReaderTask::cancelRequest(uint requestId, int reasonCode)
{
  scRequestItemMapColnIterator p = m_waitingRequests.find(requestId);
  if(p == m_waitingRequests.end()) {
    return;
  }

  scResponse response;
  
  response.clear();
  response.setRequestId(requestId);
  response.setStatus(reasonCode);
  
  handleResponse(requestId, response);  
}

// ----------------------------------------------------------------------------
// scSmplQueueModule
// ----------------------------------------------------------------------------

scSmplQueueModule::scSmplQueueModule(): scModule(), m_keepAliveTask(SC_NULL)
{
}

scSmplQueueModule::~scSmplQueueModule()
{
  if (m_keepAliveTask != SC_NULL)
    dynamic_cast<scSmplQueueKeepAliveTask *>(m_keepAliveTask)->clrParentModule(this);
}

void scSmplQueueModule::clearTask(scTask *task)
{
  if (m_keepAliveTask == task)
    m_keepAliveTask = SC_NULL;
}

scStringList scSmplQueueModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("squeue");
  return res;
}

int scSmplQueueModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;
  scString coreCmd = message->getCoreCommand();

  assert(message != SC_NULL);
  response.clearResult();

  if (
     (message->getInterface() == "squeue")
     )
  {   
  //init, listen, close, clear, get_status
    if (coreCmd == "init")
    {
      res = handleCmdInit(message, response);
    }  
    else if (coreCmd == "listen")
    {
      res = handleCmdListen(message, response);
    }  
    else if (coreCmd == "listen_at")
    {
      res = handleCmdListenAt(message, response);
    }  
    else if (coreCmd == "mark_alive")
    {
      res = handleCmdMarkAlive(message, response);
    }  
    else if (coreCmd == "get_status")
    {
      res = handleCmdGetStatus(message, response);
    }  
    else if (coreCmd == "clear")
    {
      res = handleCmdClear(message, response);
    }  
    else if (coreCmd == "close")
    {
      res = handleCmdClose(message, response);
    }  
    else if (coreCmd == "list_readers")
    {
      res = handleCmdListReaders(message, response);
    } 
    else if (coreCmd == "keep_alive")
    {
      res = handleCmdKeepAlive(message, response);
    } 
  }
  
  response.setStatus(res);
  return res;
}

scTaskIntf *scSmplQueueModule::prepareTaskForMessage(scMessage *message)
{
  scTaskIntf *res = SC_NULL;
  scString coreCmd = message->getCoreCommand();
  
  if (
     (message->getInterface() == "squeue")
     )
  {   
  //init, listen, close, clear, get_status
    if (coreCmd == "init")
    {
      res = prepareManager(message);
    }  
    else if (coreCmd == "listen")
    {
      res = prepareReader(message);
    } 
    else if (coreCmd == "keep_alive")
    {
      res = prepareKeepAliveTask(message);
    } 
  }  
  
  return res;
}

scTask *scSmplQueueModule::prepareKeepAliveTask(scMessage *message)
{
  std::auto_ptr<scSmplQueueKeepAliveTask> taskGuard;
  scSmplQueueKeepAliveTask *task;
  
  scDataNode &params = message->getParams(); 

  task = dynamic_cast<scSmplQueueKeepAliveTask *>(getKeepAliveTask());
  if (task == SC_NULL) {
    taskGuard.reset(dynamic_cast<scSmplQueueKeepAliveTask *>(createKeepAliveTask()));
    task = taskGuard.get();    
  }  

  scString qname = params.getString("queue_name");
  scString addr = params.getString("address");  
  scString target_addr = params.getString("target_address", "");  

  uint msg_limit = params.getUInt("msg_limit", 0);
  uint delay = params.getUInt("delay", 30000);
  uint error_limit = params.getUInt("error_limit", 3);
  uint error_delay = params.getUInt("error_delay", 3000);
  bool retry_listen = params.getBool("retry_listen", true);
       
  task->addJob(addr, qname, msg_limit, delay, error_limit, error_delay, retry_listen, target_addr);  

  if (task == taskGuard.get())
    taskGuard.release(); // will be returned
    
  return task;
}

scTask *scSmplQueueModule::getKeepAliveTask()
{
  return m_keepAliveTask;
}

scTask *scSmplQueueModule::createKeepAliveTask()
{
  assert(m_keepAliveTask == SC_NULL);
  m_keepAliveTask = new scSmplQueueKeepAliveTask(this);;
  return m_keepAliveTask;
}

scTask *scSmplQueueModule::prepareManager(scMessage *message)
{
  scTask *res = SC_NULL;

  scDataNode &params = message->getParams(); 
  
  if (!params.empty()) {
    scString qname;
    scString qtypeText;
    scSmplQueueType qtype;
    
    if (params.hasChild("name"))
      qname = params.getString("name");
    else
      qname = params.getString(0); 

    bool allowSenderAsReader = params.getBool("duplex", false);
    bool durable = params.getBool("durable", false);
    
    qtypeText = params.getString("type", "");
    scString faddr = params.getString("forward_to", ""); 
    uint retryLimit = params.getUInt("retry_limit", 0);    
    uint retryDelay = params.getUInt("retry_delay", 0);    
    uint contactTimeout = params.getUInt("contact_timeout", 0);    
    uint resultTimeout = params.getUInt("result_timeout", 0);  
    uint storeTimeout = params.getUInt("store_timeout", 0);  

    scDataNode extraParams;  

    extraParams.setAsParent();
    extraParams.addElement("retry_limit", scDataNode(retryLimit));
    extraParams.addElement("retry_delay", scDataNode(retryDelay));
    extraParams.addElement("contact_timeout", scDataNode(contactTimeout));
    extraParams.addElement("result_timeout", scDataNode(resultTimeout));    
    extraParams.addElement("store_timeout", scDataNode(storeTimeout));    
    
    if (!qname.empty()) {
      if (qtypeText.empty() || (qtypeText == GRD_SQUEUE_TYPE_ROUND_ROBIN))
      {
        qtype = sstRoundRobin;
      } else if (qtypeText == GRD_SQUEUE_TYPE_PULL)
      {
        qtype = sstPull;
      } else if (qtypeText == GRD_SQUEUE_TYPE_MULTICAST)
      {
        qtype = sstMultiCast;
      } else if (qtypeText == GRD_SQUEUE_TYPE_HIGHAVAIL)
      {
        qtype = sstHighAvail;
      } else if (qtypeText == GRD_SQUEUE_TYPE_FORWARD)
      {
        qtype = sstForward;
      } else if (qtypeText == GRD_SQUEUE_TYPE_NULL_DEV)
      {
        qtype = sstNullDev;
      } else {
        qtype = sstNullDev;
        throw scError(scString("Unknown queue type: ") + qtypeText);
      } 
      res = createQueue(qtype, qname, allowSenderAsReader, faddr, durable, extraParams);
    } // if qname
  } // if params  
  
  return res;
}

scTask *scSmplQueueModule::prepareReader(scMessage *message)
{ 
  std::auto_ptr<scTask> guard;

  scDataNode &params = message->getParams(); 
  
  if (!params.empty() && (params.size() > 1)) {
    scString qname, target;
    
    qname = params.getChildren().at(0).getAsString();
    target = params.getChildren().at(1).getAsString();
    
    if (!qname.empty()) {
      guard.reset(prepareReader(findQueue(qname), target));
    }
  }  
  
  if (guard.get() != SC_NULL)
    return guard.release();
  else
    return SC_NULL;  
}

scTask *scSmplQueueModule::prepareReader(scSmplQueueManagerTask *queue, const scString &target)
{ 
  std::auto_ptr<scSmplQueueReaderTask> guard;
  scSmplQueueReaderTask *res;

  guard.reset(new scSmplQueueReaderTask());
  res = guard.get();
  res->setQueueManager(queue);
  res->setTarget(target);
  
  return guard.release();
}

bool scSmplQueueModule::queueExists(const scString &name)
{
  return (findQueue(name) != SC_NULL);
}

scSmplQueueManagerTask *scSmplQueueModule::checkQueue(const scString &name)
{
  scSmplQueueManagerTask *res = findQueue(name);
  if (res == SC_NULL)
    throw scError("Unknown queue: ["+name+"]");
  return res;  
}

scSmplQueueManagerTask *scSmplQueueModule::createQueue(scSmplQueueType qtype, const scString &name, 
  bool allowSenderAsReader, const scString &forwardToAddr, bool durable, const scDataNode &extraParams)
{
  std::auto_ptr<scSmplQueueManagerTask> guard;

  if (queueExists(name))
    throw scError("Queue already exists: ["+name+"]");

  switch (qtype) {
    case sstNullDev:  
      guard.reset(new scSmplQueueManagerTaskNullDev());  
      break;
    case sstPull:  
      guard.reset(new scSmplQueueManagerTaskPull(allowSenderAsReader));  
      break;
    case sstMultiCast:  
      guard.reset(new scSmplQueueManagerTaskMultiCast(allowSenderAsReader));  
      break;
    case sstHighAvail:
      guard.reset(new scSmplQueueManagerTaskHighAvail(allowSenderAsReader, durable));  
      break;
    case sstForward:  
      guard.reset(new scSmplQueueManagerTaskForward(allowSenderAsReader));  
      break;
    default: 
    { 
      guard.reset(new scSmplQueueManagerTaskRoundRobin(allowSenderAsReader, durable));        
      scSmplQueueManagerTaskRoundRobin *task = static_cast<scSmplQueueManagerTaskRoundRobin *>(guard.get());
      task->setRetryLimit(extraParams.getUInt("retry_limit"));
      task->setRetryDelay(extraParams.getUInt("retry_delay"));
      task->setContactTimeout(extraParams.getUInt("contact_timeout"));
      task->setResultTimeout(extraParams.getUInt("result_timeout"));
      task->setStoreTimeout(extraParams.getUInt("store_timeout"));
      break;
    }  
  }    

  scSmplQueueManagerTask *res = guard.get();
  res->setName(name);
  m_managers.push_back(res);
  
  if (qtype == sstForward)
  {
    assert(!forwardToAddr.empty());
    prepareReader(guard.get(), forwardToAddr);
  }  
  
  return guard.release();
}

scSmplQueueManagerTask *scSmplQueueModule::findQueue(const scString &name)
{
  scSmplQueueManagerTask *res = SC_NULL;
  scString pname;
  
  scSmplQueueManagerList::const_iterator p;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    pname = (*p)->getName();
    if (pname == name)
    {
      res = *p;
      break;
    }  
  }  
  return res;       
}

int scSmplQueueModule::handleCmdInit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getChildren().at(0).getAsString();
    if (!qname.empty()) {
      if (queueExists(qname)) 
      {
   	    res = SC_RESP_STATUS_OK;
      } else { // no queue yet
   	    res = SC_MSG_STATUS_TASK_REQ;        
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int scSmplQueueModule::handleCmdListen(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty() && (params.hasChild("queue_name"))) {
    scString qname;    
    qname = params.getString("queue_name");
    if (!qname.empty()) {
      if (queueExists(qname)) 
      {
        scSmplQueueManagerTask *queue = findQueue(qname);
        bool readerFound = false;
        scString readerAddr;
        if (params.size() > 1) 
          readerAddr = params.getString(1);

        if (queue != SC_NULL)
          readerFound = queue->hasReader(readerAddr);
         
        if (readerFound)  
     	    res = SC_MSG_STATUS_OK;        
     	  else  
     	    res = SC_MSG_STATUS_TASK_REQ;        
      } else { // queue not found
   	    res = SC_MSG_STATUS_ERROR;
   	    setUnknownQueueError(qname, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int scSmplQueueModule::handleCmdListReaders(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getChildren().at(0).getAsString();
    if (!qname.empty()) {
      if (queueExists(qname)) 
      {
        scStringList list;
        checkQueue(qname)->getReaderList(list);
        Log::addInfo("Readers listening at: "+qname);
        
        for(scStringList::iterator i=list.begin(); i!=list.end(); ++i){
          Log::addInfo(*i);
        }   
        
   	    res = SC_RESP_STATUS_OK;        
      } else { // queue not found
   	    res = SC_MSG_STATUS_ERROR;
   	    setUnknownQueueError(qname, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int scSmplQueueModule::handleCmdMarkAlive(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty() && (params.size() > 1)) {
    if (params.size() > 1) {
      scString exec_at_addr;
      scMessageAddress exaddr, ownaddr; 
      scString queue_name, target_name;
      scDataNode newParams;
      
      if (params.hasChild("exec_at_addr"))
        exec_at_addr = params["exec_at_addr"].getAsString();
      
      if (params.hasChild("queue_name"))
      {
        queue_name = params["queue_name"].getAsString();
      
        exaddr = exec_at_addr;
        ownaddr = m_scheduler->getOwnAddress(exaddr.getProtocol());

        newParams.addChild("queue_name", new scDataNode(queue_name));
        
        if (!exec_at_addr.empty())
        {
          newParams.addChild("source_name", new scDataNode(ownaddr.getAsString()));      
          m_scheduler->postMessage(exec_at_addr, "squeue.mark_alive", &newParams);
          res = SC_MSG_STATUS_OK;
        } else {
        // perform locally
          if (params.hasChild("source_name"))
          {
            if (performMarkAlive(queue_name, params.getString("source_name")))
              res = SC_MSG_STATUS_OK;
            else
              res = SC_MSG_STATUS_ERROR;  
          } // if source exists 
        } // if local
      } // if queue name exists 
    }  // if child count > 1
  } // has children 
           
  return res;
}

bool scSmplQueueModule::performMarkAlive(const scString &queueName, const scString &srcName)
{
  bool res = false;
  scSmplQueueManagerTask *queue = findQueue(queueName);
  if (queue != SC_NULL)
  {
    res = queue->markReaderAlive(srcName);
  }  
  return res;
}

int scSmplQueueModule::handleCmdListenAt(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty() && (params.size() > 1)) {
    if (params.size() > 1) {
      scString exec_at_addr;
      scMessageAddress exaddr, ownaddr; 
      scString queue_name, target_name;
      scDataNode newParams;
      
      if (params.hasChild("exec_at_addr"))
        exec_at_addr = params["exec_at_addr"].getAsString();
      
      if (params.hasChild("queue_name"))
        queue_name = params["queue_name"].getAsString();
        
      if (params.hasChild("target_addr"))
        target_name = params.getString("target_addr");
      else {   
        exaddr = m_scheduler->evaluateAddress(exec_at_addr);
        ownaddr = m_scheduler->getOwnAddress(exaddr.getProtocol());
        target_name = ownaddr.getAsString();
      }  
      
      newParams.addChild("queue_name", new scDataNode(queue_name));
      newParams.addChild("target_name", new scDataNode(target_name));
      
      m_scheduler->postMessage(exec_at_addr, "squeue.listen", &newParams);
      res = SC_MSG_STATUS_OK;
    }  
  } // has children 
           
  return res;
}

void scSmplQueueModule::setUnknownQueueError(const scString &qname, scResponse &response)
{
  scDataNode error;
  error.setElementSafe("text", "Unknown queue: ["+qname+"]");
  response.setError(error);
}

int scSmplQueueModule::handleCmdClose(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getChildren().at(0).getAsString();
    if (!qname.empty()) {
      if (queueExists(qname)) 
      {
        closeQueue(qname);
   	    res = SC_RESP_STATUS_OK;
      } else { // queue not found
   	    res = SC_MSG_STATUS_ERROR;
   	    setUnknownQueueError(qname, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int scSmplQueueModule::handleCmdClear(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getChildren().at(0).getAsString();
    if (!qname.empty()) {
      if (queueExists(qname)) 
      {
        checkQueue(qname)->clearQueue();
   	    res = SC_RESP_STATUS_OK;
      } else { // queue not found
   	    res = SC_MSG_STATUS_ERROR;
   	    setUnknownQueueError(qname, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int scSmplQueueModule::handleCmdGetStatus(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getChildren().at(0).getAsString();
    if (!qname.empty()) {
      if (queueExists(qname)) 
      {
        scString statusDesc = checkQueue(qname)->getStatus();
        scDataNode result;
        result.setElementSafe("text", statusDesc);
        response.setResult(result);
   	    res = SC_RESP_STATUS_OK;
      } else { // queue not found
   	    res = SC_MSG_STATUS_ERROR;
   	    setUnknownQueueError(qname, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int scSmplQueueModule::handleCmdKeepAlive(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty() && (params.size() > 1)) {
    if (params.hasChild("address") && params.hasChild("queue_name"))
   	  res = SC_MSG_STATUS_TASK_REQ;        
  } // has children 
           
  return res;
}

void scSmplQueueModule::closeQueue(const scString &name)
{
  scSmplQueueManagerTask *manager = checkQueue(name);
  m_managers.remove(manager);
  manager->deleteReaders();
  scSchedulerIntf *scheduler = manager->getScheduler();
  scheduler->deleteTask(manager);
}

