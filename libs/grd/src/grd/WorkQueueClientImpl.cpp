/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueClientImpl.cpp
// Project:     grdLib
// Purpose:     Implementation of work queue client interface.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/WorkQueueClientImpl.h"
#include "perf/Log.h"
#include "perf/Timer.h"
#include "perf/Counter.h"

#include "grd/RequestHandler.h"
#include "grd/MessageConst.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

#define GRD_WORK_QUEUE_TIMERS_ENABLED
#define GRD_WORK_QUEUE_COUNTERS_ENABLED

using namespace perf;

//-------------------------------------------------------------------------------
// Private classes
//-------------------------------------------------------------------------------
class scWqRequestImpl: public scWqRequest {
public:
// construct
  scWqRequestImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget);
  virtual ~scWqRequestImpl();
// properties
  virtual bool isResultReady() const;
  virtual bool isResultOk() const;
  virtual bool getResult(scDataNode &output) const;
  virtual bool getStatus(scDataNode &output) const;
  virtual void setCommand(const scString &command);
  virtual const scString &getCommand() const;  
  virtual void setParams(const scDataNode &params);
  virtual bool getParams(scDataNode &output) const;  
  virtual void setStatus(int statusCode, const scDataNode &statusData);
  virtual void setResult(const scDataNode &resultData);
  virtual bool getHandlerName(scString &output) const;
// run
  virtual void executeAsync();
  virtual void notify();
  virtual void cancel();
  virtual void waitFor();
  virtual void throwLastError();
  virtual void checkStatus();
protected:  
  scWqRequestImpl() {}
  void yieldWait();
  int getStatusCode() const;
  const scString getWorkTargetAddr();
  scScheduler *getScheduler();
  void setHandlerName(const scString &value);
protected:   
  int m_requestId;
  int m_statusCode;
  scString m_workTarget;
  scScheduler *m_scheduler;  
  scSignal *m_waitSignal;
  scString m_command;
  scDataNode m_params;
  std::auto_ptr<scDataNode> m_result;
  std::auto_ptr<scDataNode> m_status;
  scString m_handlerName;
};

class scWqRequestGroupImpl: public scWqRequestGroup {
public:
// construct
  scWqRequestGroupImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget);
  virtual ~scWqRequestGroupImpl() {}
  virtual void waitFor();  
  virtual uint waitForAny(); // returns first ready request index that was not rdy before call
  virtual scWqRequest *newRequest();
protected:  
  scWqRequestGroupImpl() {}
  void yieldWait();
  uint indexOfReadyRequest();
protected:   
  scScheduler *m_scheduler;  
  scSignal *m_waitSignal;
  scString m_workTarget;
};

class scWaRequestHandler: public scRequestHandler {
public:
  scWaRequestHandler(scWqRequestImpl *owner);
  virtual ~scWaRequestHandler();
  virtual void handleCommError(const scString &errorText, RequestPhase phase);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
protected:
  scWqRequestImpl *m_owner;  
  cpu_ticks m_startTime;
};

//-------------------------------------------------------------------------------
// scWqRequestImpl
//-------------------------------------------------------------------------------
scWqRequestImpl::scWqRequestImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget): 
  scWqRequest(), m_scheduler(scheduler), m_waitSignal(waitSignal), m_requestId(SC_REQUEST_ID_NULL), m_statusCode(0), m_workTarget(workTarget)
{
}

scWqRequestImpl::~scWqRequestImpl()
{
}

// properties
bool scWqRequestImpl::isResultReady() const
{
  return (m_result.get() != SC_NULL);
}

bool scWqRequestImpl::isResultOk() const
{
  bool res = false;
  if (isResultReady())
    res = (getStatusCode() == SC_MSG_STATUS_OK);
  return res;  
}

bool scWqRequestImpl::getResult(scDataNode &output) const
{
  bool res = (m_result.get() != SC_NULL);
  if (res)
    output = *m_result;
  return res;    
}

bool scWqRequestImpl::getStatus(scDataNode &output) const
{
  bool res = (m_status.get() != SC_NULL);
  if (res)
    output = *m_status;
  return res;  
}

int scWqRequestImpl::getStatusCode() const
{
  if (isResultReady()) {
    return m_statusCode;
  }  
  else {
    return SC_MSG_STATUS_WAITING;  
  }  
}

void scWqRequestImpl::setStatus(int statusCode, const scDataNode &statusData)
{
  m_statusCode = statusCode;
  m_status.reset(new scDataNode(statusData));
}

bool scWqRequestImpl::getHandlerName(scString &output) const
{
  output = m_handlerName;
  return !(output.empty());
}

void scWqRequestImpl::setHandlerName(const scString &value)
{
  m_handlerName = value;
}

void scWqRequestImpl::setResult(const scDataNode &resultData)
{
  m_result.reset(new scDataNode(resultData));
  if (resultData.hasChild("_worker_id"))
    setHandlerName(resultData.getString("_worker_id"));
}

void scWqRequestImpl::setCommand(const scString &command)
{
  m_command = command;
}

const scString &scWqRequestImpl::getCommand() const
{
  return m_command;
}

void scWqRequestImpl::setParams(const scDataNode &params)
{
  m_params = params;
}

bool scWqRequestImpl::getParams(scDataNode &output) const
{
  output = m_params;
  return true;
}

const scString scWqRequestImpl::getWorkTargetAddr()
{
  return m_workTarget;
}

scScheduler *scWqRequestImpl::getScheduler()
{
  return m_scheduler;
}

// run
void scWqRequestImpl::executeAsync()
{
  scDataNode params;
  if (getParams(params)) {
    std::auto_ptr<scRequestHandler> newHandlerGuard(new scWaRequestHandler(this));
    scString addr(getWorkTargetAddr());
    m_requestId = getScheduler()->getNextRequestId();
    getScheduler()->postMessage(addr, getCommand(), &params, m_requestId, newHandlerGuard.release());
  }  
}

void scWqRequestImpl::notify()
{
  scDataNode params;
  if (getParams(params))
  {
    getScheduler()->postMessage(getWorkTargetAddr(), getCommand(), &params);
  }  
}

// - cancel work
// - disconnect request
// - set status as "aborted"
void scWqRequestImpl::cancel()
{
  if (m_requestId != SC_REQUEST_ID_NULL)
    getScheduler()->cancelRequest(m_requestId);
  setStatus(SC_MSG_STATUS_USR_ABORT, scDataNode());  
}

void scWqRequestImpl::waitFor()
{
  Timer::start("grd-cli-wait");
  while(!isResultReady())
    yieldWait();
  Timer::stop("grd-cli-wait");
}

void scWqRequestImpl::throwLastError()
{
  scDataNode status;
  getStatus(status);
  throw scError("Request error.").
    addDetails("command", scDataNode(m_command)).
    addDetails("params", scDataNode(m_params)).
    addDetails("statusCode", scDataNode(getStatusCode())).
    addDetails("status", scDataNode(status));
}

void scWqRequestImpl::checkStatus()
{
  if (!isResultReady())
    throw scError("Result not ready.").
      addDetails("command", scDataNode(m_command)).
      addDetails("params", scDataNode(m_params));
  if (!isResultOk())
    throwLastError();
}

void scWqRequestImpl::yieldWait()
{
  if (m_waitSignal != SC_NULL)
    m_waitSignal->execute();
}

//-------------------------------------------------------------------------------
// scWqRequestGroupImpl
//-------------------------------------------------------------------------------
scWqRequestGroupImpl::scWqRequestGroupImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget): 
  scWqRequestGroup(), m_scheduler(scheduler), m_waitSignal(waitSignal), m_workTarget(workTarget)
{
}

void scWqRequestGroupImpl::waitFor()
{
  scString cmdGrp;
  if (size() > 0) {
    cmdGrp = m_requests[0].getCommand();
    // add mark if there are other commands in group
    for(uint i=1, epos = size(); i != epos; i++)
      if (cmdGrp != m_requests[i].getCommand())
      {
        cmdGrp += "_and_oth";
        break;
      }
  }

  Timer::start("grd-cli-wait-wall");
  if (!cmdGrp.empty()) {
    Timer::start(scString("grd-cli-wait-wall-grp-")+cmdGrp);
    scString cmd;
    for(uint i=0, epos = size(); i != epos; i++)
    {
      if (cmd != m_requests[i].getCommand()) 
      {
        cmd = m_requests[i].getCommand();
        Timer::start(scString("grd-cli-wait-wall-cmd-")+cmd);
      }
    }
  }

  while(!isResultReady())
    yieldWait();

  if (!cmdGrp.empty()) {
    scString cmd;
    for(uint i=0, epos = size(); i != epos; i++)
    {
      if (cmd != m_requests[i].getCommand()) 
      {
        cmd = m_requests[i].getCommand();
        Timer::stop(scString("grd-cli-wait-wall-cmd-")+cmd);
      }
    }

    Timer::stop(scString("grd-cli-wait-wall-grp-")+cmdGrp);
  }

  Timer::stop("grd-cli-wait-wall");
}

uint scWqRequestGroupImpl::waitForAny()
{
  Timer::start("grd-cli-wait");
  while(!isAnyResultReady())
    yieldWait();
  Timer::stop("grd-cli-wait");
  
  return indexOfReadyRequest(); 
}

uint scWqRequestGroupImpl::indexOfReadyRequest()
{
  uint res = size();
  for(uint i=0, epos = size(); i != epos; i++)
  {
    if (isResultReady(i))
    {
      res = i;
      break;
    }  
  }
  return res;  
}

void scWqRequestGroupImpl::yieldWait()
{
  if (m_waitSignal != SC_NULL)
    m_waitSignal->execute();
}

scWqRequest *scWqRequestGroupImpl::newRequest()
{
  return new scWqRequestImpl(m_scheduler, m_waitSignal, m_workTarget);
}

// ----------------------------------------------------------------------------
// scWaRequestHandler
// ----------------------------------------------------------------------------
scWaRequestHandler::scWaRequestHandler(scWqRequestImpl *owner): m_owner(owner)
{
  assert(m_owner != SC_NULL);
  m_startTime = cpu_time_ms();
}

scWaRequestHandler::~scWaRequestHandler()
{
}

void scWaRequestHandler::handleCommError(const scString &errorText, RequestPhase phase)
{
  m_owner->setStatus(SC_RESP_STATUS_TRANSMIT_ERROR, 
    scDataNode( 
      errorText +
      scString(", phase: ") + 
      toString(static_cast<uint>(phase))
    )
  );
  m_owner->setResult(scDataNode());
  
#ifdef SC_LOG_ERRORS
  Log::addError("scWaRequestHandler::handleCommError ["+errorText+"]");
#endif  
}

void scWaRequestHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
#ifdef GRD_WORK_QUEUE_TIMERS_ENABLED
     cpu_ticks procTime = calc_cpu_time_delay(m_startTime, cpu_time_ms());
     Timer::inc("msg-proc-workq-cli", procTime);
     scString cmdTime = a_message.getCommand();
     Timer::inc("msg-proc-workq-cli-"+cmdTime, procTime);
#endif

#ifdef GRD_WORK_QUEUE_COUNTERS_ENABLED
     Counter::inc("msg-proc-workq-cli", 1);
     scString cmdCount = a_message.getCommand();
     Counter::inc("msg-proc-workq-cli-"+cmdCount, 1);
#endif

  if (a_response.isError()) {
    m_owner->setStatus(a_response.getStatus(), a_response.getError());
    m_owner->setResult(scDataNode());
  } else {
    m_owner->setStatus(a_response.getStatus(), scDataNode());
    m_owner->setResult(a_response.getResult());
  } 

#ifdef SC_LOG_ENABLED
  scString text;

  scDataNode &params = a_response.getResult(); 

  if (params.hasChild("text"))
  {
     text = params.getChildren().findByName("text").getValue().getAsString();
  }     

  Log::addText("scWaRequestHandler::handleReqResult - ["+text+"]");
#endif  
}

void scWaRequestHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  m_owner->setStatus(a_response.getStatus(), a_response.getError());
  m_owner->setResult(scDataNode());

#ifdef SC_LOG_ENABLED
  scString text;

  scDataNode &params = a_response.getError(); 

  if (params.hasChild("text"))
  {
     text = params.getChildren().findByName("text").getValue().getAsString();
  }     

  Log::addText("scWaRequestHandler::handleReqError - ["+text+"]");
#endif  
}

//-------------------------------------------------------------------------------
// scWqServerProxyImpl
//-------------------------------------------------------------------------------
scWqServerProxyImpl::scWqServerProxyImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget): 
  m_scheduler(scheduler), m_waitSignal(waitSignal), m_workTarget(workTarget)
{
}

scWqRequest *scWqServerProxyImpl::newRequest()
{
  return new scWqRequestImpl(m_scheduler, m_waitSignal, m_workTarget);
}

scWqRequestGroup *scWqServerProxyImpl::newGroup()
{
  return new scWqRequestGroupImpl(m_scheduler, m_waitSignal, m_workTarget);
}

