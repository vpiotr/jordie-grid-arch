/////////////////////////////////////////////////////////////////////////////
// Name:        SchedulerImpl.cpp
// Project:     grdLib
// Purpose:     Scheduler interface implementation.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include <set>

#include "sc/dtypes.h"
#include "sc/proc/process.h"

#include "perf/Log.h"
#include "perf/time_utils.h"
#include "perf/Timer.h"
#include "perf/Counter.h"

#include "grd/details/SchedulerImpl.h"
#include "grd/details/MessageGateInproc.h"
#include "grd/MessageConst.h"
#include "grd/ModuleImpl.h"
#include "grd/TaskImpl.h"
#include "grd/NodeFactory.h"
#include "grd/details/ResolveHandler.h"
#include "grd/EnvSerializerJsonYajl.h"
#include "grd/MessageTrace.h"
#include "base\wildcard.h"

using namespace perf;
using namespace dtp;
using namespace proc;

//#define GRD_TRACE_SCHEDULER_TIME
//#define GRD_TRACE_SCHEDULER_TIME_FOR_CMD

#define GRD_TRACE_SCHEDULER_COUNTER
#define GRD_TRACE_SCHEDULER_COUNTER_FOR_CMD

const uint DEF_CLEANUP_DELAY = 10000; // msec

// ----------------------------------------------------------------------------
// local classes
// ----------------------------------------------------------------------------
class scSchedulerImpl: public scScheduler
{
  public:
    using scScheduler::handleResolveFailed;  
  protected:  
    virtual bool resolveDestForMessage(const scString &address, const scString &command, 
      const scDataNode *params, int requestId, scRequestHandler *handler);
};

class scCommandMap: public scCommandMapIntf
{
public:
  virtual void registerCommandMap(const scString &cmdFilter, const scString &targetName, int priority) {
    m_commandFilterMap.insert(CommandFilterMap::value_type(priority, std::make_pair(cmdFilter, targetName)));
  }

  virtual bool findTargetForCommand(const scString &command, scString &target) {
    bool res = false;
    for(CommandFilterMap::const_iterator it = m_commandFilterMap.cbegin(), epos = m_commandFilterMap.cend(); it != epos; ++it)
    {
      if (wildcardMatch(command, it->second.first)) {
        target = it->second.second;
        res = true;
        break;
      }
    }
    return res;
  }

private:
  typedef std::pair<scString, scString> CommandFilterEntry; 
  typedef std::map<int, CommandFilterEntry> CommandFilterMap;
  CommandFilterMap m_commandFilterMap;
};


// ----------------------------------------------------------------------------
// scScheduler
// ----------------------------------------------------------------------------
scScheduler::scScheduler() 
{
  m_nextTaskId = 1;
  m_nextRequestId = 1;
  m_status = ssCreated;
  m_defInputGate = m_defOutputGate = SC_NULL;  
  m_lastCleanupTime = 0;
  m_features = 0;
  m_commandMap.reset(new scCommandMap());

  // register default mapping - all messages without address will go to @worker alias (rule has lowest priority)
  m_commandMap->registerCommandMap("*.*", "@worker", INT_MAX);
}

scScheduler::~scScheduler()
{
  Log::addDebug("Destroying scheduler");
}

scScheduler *scScheduler::newScheduler()
{
  return new scSchedulerImpl();
}

void scScheduler::init() 
{
  m_registry.registerNodeForRole(getOwnAddress(), SC_ADDR_THIS);
  prepareDefaultGates();
  registerSelf();
  m_status = ssRunning;
}

void scScheduler::checkCleanup()
{
  if (!m_lastCleanupTime)
    m_lastCleanupTime = cpu_time_ms();
  else if (is_cpu_time_elapsed_ms(m_lastCleanupTime, DEF_CLEANUP_DELAY))
  {
    performCleanup();
    m_lastCleanupTime = cpu_time_ms();
  }  
}

void scScheduler::performCleanup()
{
  m_registry.validateEntries();
}

void scScheduler::prepareDefaultGates() 
{
  scMessageGateInproc *newGate;
  
  newGate = new scMessageGateInprocIn();
  newGate->setOwnerName(getName());
  newGate->setLocalRegistry(m_localRegistry);
  m_inputGates.push_back(newGate);
  m_defInputGate = newGate;
    
  newGate = new scMessageGateInprocOut();
  newGate->setOwnerName(getName());
  newGate->setLocalRegistry(m_localRegistry);
  m_outputGates.push_back(newGate);
  m_defOutputGate = newGate;
}

unsigned int scScheduler::getNextTaskId() 
{
  unsigned int res = m_nextTaskId;
  ++m_nextTaskId;
  return res;
}

int scScheduler::getNextRequestId() 
{
  int res = m_nextRequestId;
  
  if (m_nextRequestId == INT_MAX)
    m_nextRequestId = 1;
  else  
    ++m_nextRequestId;

  return res;
}

// find address list from registry & post message
void scScheduler::postMessage(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandler *handler)
{  
  scRequestHandlerTransporter transporter;
  try {    
    if (handler != SC_NULL)
      transporter.reset(handler);

    scString realAddress(address);

    if (address.empty())
    {
      scString filterTarget;
      if (m_commandMap->findTargetForCommand(command, filterTarget))
        realAddress = filterTarget;
    }

    bool unknownAlias;
    scStringList addressList = getAddrList(realAddress, unknownAlias);

    if (unknownAlias || addressList.empty()) {
      bool forResolve = false;

      if (!m_directoryAddr.empty())
        forResolve = resolveDestForMessage(realAddress, command, params, requestId, handler);

      if (!forResolve)  
        if (!forwardMessage(realAddress, command, params, requestId, handler))
          throw scError("Unknown receiver: ["+address+"]");      
    } else { 
    // adress found    
      if ((requestId != SC_REQUEST_ID_NULL) && (handler == SC_NULL))
        throw scError("No handler provided for request handling");
      
      // for each address:
      for(scStringList::iterator i=addressList.begin(); i!=addressList.end(); ++i){
        postMessageForAddress((*i), command, params, requestId, transporter); 
      }   
    }
  }
  catch(scError &excp) {
    if ((handler == SC_NULL) || (!handler->handleException(excp)))
      throw;
  }
  
#ifdef SC_LOG_ENABLED
   Log::addText("postMessage executed for: ["+command+"]"); 
#endif     
}      

// post message to known address
void scScheduler::postMessageForAddress(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandlerTransporter &transporter)
{
#ifdef DEBUG_LOG_MSGS
  Log::addDebug("Sending message ["+command+"], to: ["+address+"]");
#endif  

  scMessageAddress tempAddr(address);

  if (address.empty())
  {
    scString filterTarget;
    if (m_commandMap->findTargetForCommand(command, filterTarget))
      tempAddr = scMessageAddress(filterTarget);
  }

  scMessageAddress myaddr = getOwnAddress(tempAddr.getProtocol());
  scMessageGate *gate;

  scMessageTrace::addTrace("req_send_prep",myaddr.getAsString(), tempAddr.getAsString(), requestId, command);

  myaddr.setProtocol(tempAddr.getProtocol());
            
  if (isOwnAddress(tempAddr))
  {
    gate = &findInpGateForThis();
  }  
  else   
  {
    gate = &findOutGateForProtocol(tempAddr.getProtocol());
  }
  
  scEvent *myevent = new scMessage(command, params, requestId);
  scEnvelope *envelope = new scEnvelope(myaddr, tempAddr, myevent);  
  try     
  {
     // create requestItem if we wait for result
     //if (!isOwnAddress(tempAddr))
     //{
        if (requestId != SC_REQUEST_ID_NULL) {
          //was:m_waitingMessages.push_back(new scEnvelope(*envelope));
          m_waitingMessages.insert(requestId, new scRequestItem(*envelope, transporter));
          notifyObserversMsgWaitStarted(*envelope, requestId);
        }  
     //}  
  
     if (transporter.get() != SC_NULL) 
       transporter.get()->beforeReqQueued(*envelope);  

     notifyObserversMsgReadyForSend(*envelope, *gate);
     gate->put(envelope);
  }
    
  catch (...) {
    delete envelope;
    throw;
  }  
}     

bool scScheduler::forwardMessage(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandler *handler)
{
  bool res = false;
  scString target;

  if (!m_dispatcher.empty())
    target = m_dispatcher;
  else
    target = getOwnAddress().getAsString();
      
  if (!target.empty()) {
    scDataNode newParams;
    newParams.addChild("address", new scDataNode(address));
    newParams.addChild("fwd_command", new scDataNode(command));
    if (params != SC_NULL)
      newParams.addChild("fwd_params", new scDataNode(*params));

#ifdef DEBUG_LOG_MSGS
    Log::addDebug("Forwarding message to: "+address+", command: "+command);
#endif      
    postMessage(target, "core.forward", &newParams, requestId, handler);  
    res = true;
  } else {
    // make sure handler is referenced - just in case (not required)
    scRequestHandlerTransporter transporter(handler);
  }
  return res;
}

bool scScheduler::resolveDestForMessage(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandler *handler)
{
  // make sure handler is referenced - just in case (not required)
  scRequestHandlerTransporter transporter(handler);
  return false;
}

void scScheduler::handleResolveFailed(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandler *handler)
{
  if (!forwardMessage(address, command, params, requestId, handler))
  {
    Log::addError(scString("Unknown receiver") + " [" + address + "]");
    if (handler != SC_NULL)
    {
      scMessage dummyMessage;
      scResponse response;
      response.setError(scDataNode(scString("Error - unknown receiver: [") + address + "]"));
      handler->handleReqError(dummyMessage, response);
    }  
  }  
}

void scScheduler::postEnvelopeForThis(scEnvelope *envelope)
{  
  scMessageGate *gate;
  
  gate = &findInpGateForThis();  
  notifyObserversMsgReadyForSend(*envelope, *gate);
  gate->put(envelope);  
}
 
scMessageAddress scScheduler::getOwnAddress(const scString &protocol) 
{
  scMessageAddress addr;
  bool found = false;

  //for(scMessageGateColnIterator i=m_outputGates.begin(); i!=m_outputGates.end(); ++i)
  //{
  //  if (i->supportsProtocol(protocol))
  //  {
  //    if (i->getOwnAddress(protocol, addr))
  //    {
  //      found = true;
  //      break;
  //    }
  //  }
  //}      

  for(scMessageGateColnIterator i=m_inputGates.begin(); i!=m_inputGates.end(); ++i)
  {
    if (i->supportsProtocol(protocol))
    {
      if (i->getOwnAddress(protocol, addr))
      {
        found = true;
        break;
      }
    }
  }      

  if (!found)
  {
    addr.clear();  
    addr.set("#/"+getName());
  }  
  return addr;
}

bool scScheduler::isOwnAddress(const scMessageAddress &value) 
{ 
  if ((value.getRole() == SC_ADDR_THIS) || 
      (value.getHost().empty() && 
       value.getNode() == getName())
     )
  {
    return true;
  } else {
    scMessageAddress addr = value;
    if (getOwnAddress(addr.getProtocol()).getAsString() == value.getAsString())
      return true;
    else  
      return false;
  }            
}

bool scScheduler::isOwnAddressSkipTask(const scMessageAddress &value) 
{ 
  if ((value.getRole() == SC_ADDR_THIS) || 
      (value.getHost().empty() && 
       value.getNode() == getName())
     )
  {
    return true;
  } else {
    scMessageAddress addr = value;
    addr.setTask("");
    scMessageAddress ownAddr = getOwnAddress(addr.getProtocol());
    ownAddr.setTask("");
    if (ownAddr.getAsString() == addr.getAsString())
      return true;
    else  
      return false;
  }            
}

scStringList scScheduler::getAddrList(const scString &address, bool &unknownAlias) 
{
  if (isIndirectAddress(address) && isOwnAddressSkipTask(address))
  {
    scMessageAddress addr = address;
    scString task = addr.getTask();
    return m_registry.getAddrList(task, unknownAlias);
  } else {  
    return m_registry.getAddrList(address, unknownAlias);
  }  
}

// change virtual task name to real value
scString scScheduler::resolveTaskName(const scString &address)
{
    bool unknownAlias;
    scStringList list = getAddrList(address, unknownAlias);
    if (!list.empty())
    {
      return list.front();
    } else {
      return "";
    }
}

bool scScheduler::isIndirectAddress(const scString &address) 
{
  scMessageAddress addr = address;
  scString task = addr.getTask();  
  if (
       (
          (addr.getFormat() == scMessageAddress::AdrFmtDefault) 
          || 
          (addr.getFormat() == scMessageAddress::AdrFmtRaw)
       ) 
       && 
       scMessageAddress::isRoleName(task)
     )
    return true;
  else
    return false;  
}

void scScheduler::postEnvelope(scEnvelope *envelope,
  scRequestHandler *handler) 
{
  bool unknownAlias;
  //scString receiverAddr;

  if (envelope->getReceiver().getAsString().empty() && !envelope->getEvent()->isResponse())
  {
    scString filterTarget;
    scMessage *msg = static_cast<scMessage *>(envelope->getEvent());

    if (m_commandMap->findTargetForCommand(msg->getCommand(), filterTarget))
    {
      envelope->setReceiver(scMessageAddress(filterTarget));
    }
  }

  scStringList addressList = getAddrList(envelope->getReceiver().getAsString(), unknownAlias);

  if (unknownAlias || addressList.empty()) {
    scString addr = envelope->getReceiver().getAsString();
    if (!forwardEnvelope(envelope, handler)) {
      delete envelope;
      throw scError("Unknown receiver: ["+addr+"]");
    }  
  } else { 
    if (addressList.size() > 1) {
      boost::shared_ptr<scEnvelope> guard(envelope);      
      for(scStringList::iterator i=addressList.begin(); i!=addressList.end(); ++i){
        postEnvelopeForAddress((*i), new scEnvelope(*envelope), handler); 
      }     
    } else {
      postEnvelopeForAddress(addressList.front(), envelope, handler);     
    }
  }
}

scString scScheduler::evaluateAddress(const scString &virtualAddr)
{
  bool unknownAlias;
  scString res;
  scStringList addressList = getAddrList(virtualAddr, unknownAlias);

  if (unknownAlias || addressList.empty()) 
    res = virtualAddr;
  else 
    res = addressList.front();
  
  return res;  
}

bool scScheduler::forwardEnvelope(scEnvelope *envelope,
  scRequestHandler *handler) 
{
  boost::shared_ptr<scEnvelope> guard(envelope);      
  scRequestHandlerTransporter transporter(handler);
  
  bool res = false;  
  if (m_dispatcher.length() && !envelope->getEvent()->isResponse()) {
    scMessage *message = dynamic_cast<scMessage *> (envelope->getEvent());
    scDataNode newParams;
    newParams.addChild("address", new scDataNode(envelope->getReceiver().getAsString()));
    newParams.addChild("fwd_command", new scDataNode(message->getCommand()));
    if (!message->getParams().isNull())
      newParams.addChild("fwd_params", new scDataNode(message->getParams()));

#ifdef DEBUG_LOG_MSGS
    Log::addDebug("Forwarding message to: "+
      envelope->getReceiver().getAsString()+
      ", by: "+m_dispatcher+      
      ",command: "+message->getCommand());
#endif      
    postMessage(m_dispatcher, "core.forward", &newParams, message->getRequestId(), handler);  
    res = true;
  }
  return res;
}
  
void scScheduler::postEnvelopeForAddress(const scString &address,
  scEnvelope *envelope, scRequestHandler *handler) 
{
  scMessageAddress receiver = address;
  scRequestHandlerTransporter transporter;
  int requestId = envelope->getEvent()->getRequestId();
  
  if (envelope->getSender().isEmpty() || 
      (envelope->getSender().getProtocol() != receiver.getProtocol())
     ) 
  {
    envelope->setSender(getOwnAddress(receiver.getProtocol()));  
  }  
  envelope->setReceiver(receiver);
    
  if (handler != SC_NULL)
  {
    transporter.reset(handler);
    if (requestId == SC_REQUEST_ID_NULL)
      throw scError("Request Id required for handler");
  }  

  if (handler != SC_NULL) 
    handler->beforeReqQueued(*envelope);  

  if ((requestId != SC_REQUEST_ID_NULL) && !envelope->getEvent()->isResponse()) {
    m_waitingMessages.insert(requestId, new scRequestItem(*envelope, transporter));
    notifyObserversMsgWaitStarted(*envelope, requestId);
  }    

  if (isOwnAddress(receiver))
  {
    postEnvelopeForThis(envelope); 
  } else {    
    scMessageGate &gate = findOutGateForProtocol(receiver.getProtocol());       
    notifyObserversMsgReadyForSend(*envelope, gate);
    gate.put(envelope);  
  }  
}


scMessageGate &scScheduler::findOutGateForProtocol(const scString &protocol) 
{
  if (protocol.empty()) {
    if (!m_defOutputGate)
      throw scError("No gate registered");    
    return *m_defOutputGate;
  } else {
    // for each address:
    for(scMessageGateColnIterator i=m_outputGates.begin(); i!=m_outputGates.end(); ++i){
      if ((*i).supportsProtocol(protocol))
        return (*i);
    }   
    // not found
    std::string str = protocol;
    throw scError("No gate found for protocol: "+str);    
  }
}

scMessageGate &scScheduler::findInpGateForThis() 
{
 if (!m_defInputGate) 
   throw scError("No input gate registered");    

 return *m_defInputGate;
}

void scScheduler::addModule(scModuleIntf *a_handler) 
{
  m_modules.push_back(a_handler);  
  dynamic_cast<scModule *>(a_handler)->setScheduler(this);
}

void scScheduler::addInputGate(scMessageGate *a_gate)
{
  m_inputGates.push_back(a_gate);
  a_gate->setOwner(this);
  a_gate->init();
}

void scScheduler::addOutputGate(scMessageGate *a_gate)
{
  m_outputGates.push_back(a_gate);
  a_gate->setOwner(this);
  a_gate->init();
}

// handle gates, messages and tasks
void scScheduler::run() 
{
  if ((m_status == ssRunning) || (m_status == ssStopping))
  {
    checkCleanup();
    runGates();
    runMessages();
    checkTimeouts();
    runTasks();
  }
  checkClose();
} 

bool scScheduler::needsRun()
{
  return (!gatesEmpty() || tasksNeedsRun());
}

void scScheduler::checkClose()
{
  if (m_status == ssStopping)
  if (m_tasks.size() == 0)
    setStatus(ssStopped);
}

void scScheduler::flushEvents()
{
  runMessages();
}

void scScheduler::runGates() 
{
  // input
  for(scMessageGateColnIterator i=m_inputGates.begin(); i!=m_inputGates.end(); ++i){
    i->run();
  }   
  // output
  for(scMessageGateColnIterator i=m_outputGates.begin(); i!=m_outputGates.end(); ++i){
    i->run();
  }     
} 

bool scScheduler::gatesEmpty() 
{
  bool res = true;
  
  // input
  for(scMessageGateColnIterator i=m_inputGates.begin(); i!=m_inputGates.end(); ++i){
    if (!i->empty())
    {
      res = false;
      break;
    }
  }   
  // output
  if (res)
  for(scMessageGateColnIterator i=m_outputGates.begin(); i!=m_outputGates.end(); ++i){
    if (!i->empty())
    {
      res = false;
      break;
    }
  }     
  
  return res;
} 

void scScheduler::runMessages() 
{
  boost::shared_ptr<scEnvelope> guard;      
  
  // input
  for(scMessageGateColnIterator i=m_inputGates.begin(); i!=m_inputGates.end(); ++i){
    while(!i->empty()) 
    {
      guard.reset(i->get());
      assert(guard->getEvent() != SC_NULL);
      
      if (!guard->getEvent()->isResponse())
      {
        intDispatchMessage(*guard);
      }  
      else
      {
        handleResponse(*guard);  
      }  
      guard.reset();
    } // while   
  } // for
} 

void scScheduler::runTasks()
{
  scTaskColnIterator pnext;
  std::set<scString> taskNames;
  scTaskColnIterator taskPos;
  
  // tasks can disappear during this loop
  for(scTaskColnIterator i=m_tasks.begin(); i!=m_tasks.end(); i++) {
    taskNames.insert(i->getName());
  }  
  
  for(std::set<scString>::iterator it = taskNames.begin(); it != taskNames.end(); it++) {
    taskPos = findTask(*it);
    if (taskPos != m_tasks.end())
      taskPos->run();
  }
} 

bool scScheduler::tasksNeedsRun()
{
  bool res = false;
  for(scTaskColnIterator i=m_tasks.begin(); i!=m_tasks.end(); ++i) {
    res = res || i->needsRun();
  }
  return res;
}

void scScheduler::requestStop()
{
  scSchedulerStatus currStatus = getStatus();
  if ((currStatus == ssRunning) || (currStatus == ssCreated))
  {
    setStatus(ssStopping);
    for(scTaskColnIterator i=m_tasks.begin(); i!=m_tasks.end(); ++i) {
      i->requestStop();
    }
    if (!m_tasks.size())
      setStatus(ssStopped);
  }
}

// dispatch local message (without sender/receiver), = Win32 "send"
int scScheduler::dispatchMessage(const scMessage &message, scResponse &response)
{
  int res;
  
  //try {
    std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
    envelopeGuard->setReceiver(scMessageAddress(getOwnAddress()));
    
    std::auto_ptr<scMessage> messageGuard(new scMessage(message));
    envelopeGuard->setEvent(messageGuard.release());  
    res = intDispatchMessage(*envelopeGuard, &response);
  //}
  //catch(const std::exception& e) {
  //  notifyObserversMsgHandleError(message, response, e);
  //  scString msg = scString("msg dispatch error - [")+e.what()+"]";
  //  scDataNode newError;  

  //  response.clearResult();
  //  newError.addChild(new scDataNode("text", msg));
  //  response.setError(newError);
  //  res = SC_MSG_STATUS_EXCEPTION;
  //}
  
  response.setStatus(res);
  return res;
}

// dispatch message received from input queue 
int scScheduler::intDispatchMessage(const scEnvelope &envelope, scResponse *a_response) 
{
  int status;
  scMessage &message = *dynamic_cast<scMessage *>(envelope.getEvent());
  //notifyObserversMsgArrived(message);
  notifyObserversEnvArrived(envelope);
  try {
    scMessageTrace::addTrace("req_recv",envelope.getSender().getAsString(), envelope.getReceiver().getAsString(), message.getRequestId(), message.getCommand());

    status = dispatchMessageForTasks(envelope, a_response);
    if ((status == SC_MSG_STATUS_PASS) || (status == SC_MSG_STATUS_UNK_MSG))
      status = dispatchMessageForModules(envelope, a_response);  
    if ((status != SC_MSG_STATUS_OK) && (status != SC_MSG_STATUS_PASS) && (status != SC_MSG_STATUS_FORWARDED))
      handleDispatchError(status, envelope); 

    scMessageTrace::addTrace("req_dispatched",envelope.getSender().getAsString(), envelope.getReceiver().getAsString(), message.getRequestId(), message.getCommand());
    notifyObserversMsgHandled(message, *a_response, status);
  } 
  catch(const std::exception& e) {
    notifyObserversMsgHandleError(message, *a_response, e);
    std::auto_ptr<scResponse> responseGuard;
    scResponse *usedResponse;

    if (a_response != SC_NULL)
        usedResponse = a_response;
    else {
        responseGuard.reset(new scResponse);
        usedResponse = responseGuard.get();
    }

    scString msg = scString("msg dispatch error - [")+e.what()+"]";
    scDataNode newError;  

    usedResponse->clearResult();
    newError.addChild("text", new scDataNode(msg));
    usedResponse->setError(newError);
    status = SC_MSG_STATUS_EXCEPTION;
    usedResponse->setStatus(status);

    if (a_response == SC_NULL) {
       checkPostResponse(envelope, *usedResponse);
    }

    Log::addError(msg);
  }  

  return status;  
}

void scScheduler::throwDispatchError(int status, const scEnvelope &envelope) 
{
  scString msg;
  msg = "Unknown message: "+((scMessage *)envelope.getEvent())->getCommand(); 
  msg += ", status: "+toString(status);
  throw scError(msg);
}

scEnvelope *scScheduler::createErrorResponseFor(const scEnvelope &srcEnvelope, const scString &msg, int a_status)
{
  scEnvelope *newEnvelope = new scEnvelope();
  scResponse *newResponse = new scResponse(); 
  scDataNode newError;  

  newResponse->clearResult();
  newError.addChild("text", new scDataNode(msg));
  newResponse->setError(newError);
  newResponse->setStatus(a_status);
  newResponse->setRequestId(srcEnvelope.getEvent()->getRequestId());
  
  newEnvelope->setEvent(newResponse);  
  newEnvelope->setReceiver(srcEnvelope.getSender());
  newEnvelope->setSender(srcEnvelope.getReceiver());
  
  return newEnvelope;
}

void scScheduler::handleDispatchError(int status, const scEnvelope &envelope)
{
  scEnvelope* renvelope;

  if (!envelope.getEvent()->isResponse())
  {
    scString msg;
    msg = "Dispatch error for: "+((scMessage *)envelope.getEvent())->getCommand(); 
    msg += ", status: "+toString(status);
  
    renvelope = createErrorResponseFor(envelope, msg, status);
          
    postEnvelope(renvelope);
  } else {
    throw scError("Unknown receiver for response: ["+(envelope.getReceiver().getAsString())+"]");
  }  
}

int scScheduler::dispatchMessageForTasks(const scEnvelope &envelope, scResponse *a_response)
{
  int res = SC_MSG_STATUS_UNK_MSG;

  if (!(envelope.getReceiver().getTask().empty()))
  {
    res = dispatchMessageForOneTask(envelope, a_response);
  }  
  
  return res;
}

//@not used anymore:
//int scScheduler::dispatchMessageForAnyTask(scEnvelope *envelope)
//{
//  int res = SC_MSG_STATUS_UNK_MSG;
//
//  if (!m_tasks.empty())
//  {
//    int task_res;
//    scResponse response;
//
//    for(scTaskColnIterator i=m_tasks.begin(); i!=m_tasks.end(); ++i){      
//      task_res = i->handleMessage(*envelope, response);
//      //task_res = i->handleMessage(dynamic_cast<scMessage *> (envelope->getEvent()), response);
//
//      if (task_res == SC_MSG_STATUS_OK) 
//        checkPostResponse(*envelope, response);
//      
//      if (task_res != SC_MSG_STATUS_PASS) 
//      { // error
//        res = task_res;
//        break;
//      }
//      response.clear();
//    }    
//  }
//  return res; 
//}

int scScheduler::dispatchMessageForOneTask(const scEnvelope &envelope, scResponse *a_response)
{
  int res;
  scResponse response;
  scString taskName = envelope.getReceiver().getTask();
  scTaskColnIterator found = findTask(taskName);

  if (found == m_tasks.end()) {
      scString receiver = envelope.getReceiver().getAsString();
      scString receiverNew = resolveTaskName(receiver);
      if (isOwnAddress(receiverNew)) {
          scMessageAddress taskAddress(receiverNew);
          taskName = taskAddress.getTask();
          if (!taskName.empty())
            found = findTask(taskName);
      }
  }

  if (found == m_tasks.end()) {
    res = SC_MSG_STATUS_UNK_TASK;
  } else {    
    if (!envelope.getEvent()->isResponse()) {
      scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
      response.initFor(*message);    
    }  
    res = found->handleMessage(const_cast<scEnvelope &>(envelope), response);
    response.setStatus(res);

    if (res == SC_MSG_STATUS_OK) 
    { 
      if (a_response == SC_NULL)
        checkPostResponse(envelope, response);
      else
        *a_response = response;         
    }    
  }  
  return res; 
}

int scScheduler::dispatchMessageForModules(const scEnvelope &envelope, scResponse *a_response)
{
  int res;
  scMessage *message;
  scString intf;
  message = dynamic_cast<scMessage *> (envelope.getEvent());
  assert(message != SC_NULL);
  intf = message->getInterface();

  res = SC_MSG_STATUS_UNK_MSG;
  if (intf.length())
    res = dispatchMessageForModulesByIntf(envelope, a_response);
  if ((res == SC_MSG_STATUS_PASS) || (res == SC_MSG_STATUS_UNK_MSG))  
    res = dispatchMessageForModulesDirect(envelope, a_response);
  return res;  
}  

int scScheduler::dispatchMessageForModulesDirect(const scEnvelope &envelope, scResponse *a_response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  int hnd_res;
  scResponse response, new_response;
  scMessage *message;
  scModuleIntf *module;

  message = dynamic_cast<scMessage *> (envelope.getEvent());
  
  new_response.initFor(*message);          

  for(scModuleListIterator i=m_modules.begin(); i!=m_modules.end(); ++i){
    module = *i;
    response = new_response;
    hnd_res = handleMessageByModule(*module, envelope, response, (a_response == SC_NULL)); 
    
    if ((hnd_res != SC_MSG_STATUS_PASS) && (hnd_res != SC_MSG_STATUS_UNK_MSG)) 
    { // handled
      res = hnd_res;
      break;
    }
  } 

  if (a_response != SC_NULL)
  {
    *a_response = response;
  }      

  return res;    
}

int scScheduler::dispatchMessageForModulesByIntf(const scEnvelope &envelope, scResponse *a_response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  int hnd_res;
  scString intf;
  scResponse response;
  scMessage *message;

  message = dynamic_cast<scMessage *> (envelope.getEvent());
  intf = message->getInterface();

  for(scModuleListIterator i=m_modules.begin(); i!=m_modules.end(); ++i){
    if (!(*i)->supportsInterface(intf))
      continue;
    response.initFor(*message);          
    hnd_res = handleMessageByModule(**i, envelope, response, (a_response == SC_NULL)); 
    
    if ((hnd_res != SC_MSG_STATUS_PASS) && (hnd_res != SC_MSG_STATUS_UNK_MSG)) 
    { // handled
      res = hnd_res;
      break;
    }
    response.clear();
  } 

  if (a_response != SC_NULL)
  {
    *a_response = response;
  }      

  return res;    
}

int scScheduler::handleMessageByModule(scModuleIntf &handler, const scEnvelope &envelope, scResponse &response, bool postResponse)
{
  int hnd_res;
  scTaskIntf *newTask;
  scMessage *message;
  
  message = dynamic_cast<scMessage *> (envelope.getEvent());
  
  hnd_res = handler.handleMessage(envelope, response);
  //hnd_res = handler.handleMessage(message, response);
  
  if (hnd_res == SC_MSG_STATUS_TASK_REQ) 
  { 
    newTask = handler.prepareTaskForMessage(message);
    if (newTask != SC_NULL) 
    {
      addTask(newTask);
      hnd_res = SC_MSG_STATUS_OK;
    }
  }  
  
  response.setStatus(hnd_res);

  if (postResponse)
    checkPostResponse(envelope, response);

  return hnd_res;
}

void scScheduler::addTask(scTaskIntf *a_task)
{
  if (!a_task->getName().length() || taskExists(a_task->getName())) {
    scString newName;
    do {
      newName = "A"+toString(getNextTaskId());
    } while (taskExists(newName));  
    a_task->setName(newName);
  }  

  scTask *task = dynamic_cast<scTask *>(a_task);

  if (task != NULL)
    task->setScheduler(this);

  m_tasks.push_back(a_task);

  if (task != NULL)
    task->init();
}

scTaskColnIterator scScheduler::findTask(const scString &name)
{
  scTaskColnIterator p;
  
  for(p=m_tasks.begin(); p!=m_tasks.end(); ++p) {
    if (p->getName() == name) 
      break;
  }
  
  return p;
}  

uint scScheduler::getNonDeamonTaskCount()
{
  uint res = 0;
  scTaskColnIterator p;
  
  for(p=m_tasks.begin(); p!=m_tasks.end(); ++p) {
    if (!p->isDaemon()) 
      res++;
  }
  
  return res;
}

bool scScheduler::taskExists(const scString &a_name)
{
  scTaskColn::iterator p = findTask(a_name);
  return (p != m_tasks.end());  
}

void scScheduler::deleteTask(scTaskIntf *a_task)
{
  scTaskColn::iterator p = findTask(a_task->getName());
  if (p != m_tasks.end())    
  {
    notifyHandlersTaskDelete(a_task);
    m_tasks.release(p);
  }  
}

void scScheduler::notifyHandlersTaskDelete(scTaskIntf *a_task)
{
  scRequestItem foundItem;
  scRequestItemMapColnIterator p, pold;
  bool handlerForDelete;
  
  for(p = m_waitingMessages.begin(); p != m_waitingMessages.end(); ) 
  {    
    foundItem = *p->second;
    handlerForDelete = false;
    if (foundItem.getHandler())
      foundItem.getHandler()->beforeTaskDelete(a_task, handlerForDelete);
    
    if (handlerForDelete) {
//      pold = p;
//      pold++;
      if (foundItem.getEnvelope() != SC_NULL)
      {
        notifyObserversMsgWaitEnd(foundItem.getRequestId());
      }  
      p = m_waitingMessages.erase(p);
//      p = pold;
    }  
    else
      p++;
  }
}

scTaskIntf *scScheduler::extractTask(scTaskIntf *a_task)
{
  scTaskColnIterator p = findTask(a_task->getName());

  if (p != m_tasks.end()) 
  {
    scTaskColn::auto_type transporter = m_tasks.release(p);    
    return transporter.release();
  } else {
    return SC_NULL;
  }  
}

bool scScheduler::checkPostResponse(const scEnvelope &orgEnvelope, const scResponse &response)
{
   bool res;
   scMessage *message = dynamic_cast<scMessage *> (orgEnvelope.getEvent());
   res = (message->getRequestId()!= SC_REQUEST_ID_NULL);
   if (res)
     postResponse(orgEnvelope, response);
   return res;  
}

void scScheduler::postResponse(const scEnvelope &orgEnvelope, const scResponse &response)
{   
   scResponse *newResponse = new scResponse(response);
   scEnvelope *newEnvelope = new scEnvelope();
   scMessage *message = dynamic_cast<scMessage *> (orgEnvelope.getEvent());
   
   newEnvelope->setSender(orgEnvelope.getReceiver());
   newEnvelope->setReceiver(orgEnvelope.getSender());
   newResponse->setRequestId(message->getRequestId());
   newEnvelope->setEvent(newResponse);
   
   scMessageTrace::addTrace("resp_rdy", newEnvelope->getSender().getAsString(), newEnvelope->getReceiver().getAsString(), message->getRequestId(), message->getCommand());

   postEnvelope(newEnvelope);

   scMessageTrace::addTrace("resp_put", newEnvelope->getSender().getAsString(), newEnvelope->getReceiver().getAsString(), message->getRequestId(), message->getCommand());
   
#ifdef SC_LOG_ENABLED
   Log::addText("postResponse executed"); 
#endif        
}

void scScheduler::handleResponse(const scEnvelope &envelope)
{
  scRequestItem reqItem;
  scEnvelope *orgEnvelope;
  
#ifdef DEBUG_LOG_MSGS
  Log::addText("Response received from ["+envelope->getSender().getAsString()+"]");  
#endif

  if (matchResponse(envelope, reqItem))
  {
    scMessage *msg = dynamic_cast<scMessage *>(reqItem.getEnvelope()->getEvent());
    scResponse *traceResponse = dynamic_cast<scResponse *>(envelope.getEvent());
    dtpString respEvent("resp_recv");

    if (traceResponse != NULL)
      respEvent += dtpString("_")+(traceResponse->isError()?"err":"ok");

    if (msg) 
      scMessageTrace::addTrace(respEvent,envelope.getSender().getAsString(), envelope.getReceiver().getAsString(), reqItem.getRequestId(),msg->getCommand());
    else
      scMessageTrace::addTrace(respEvent,envelope.getSender().getAsString(), envelope.getReceiver().getAsString(), reqItem.getRequestId(),"?");

    notifyObserversResponseArrived(envelope, reqItem);
    try {
      orgEnvelope = reqItem.getEnvelope();
      scTaskIntf *matchTask = findTask(envelope.getReceiver());
      if ( matchTask != SC_NULL )
      {
        scResponse *response = dynamic_cast<scResponse *>(envelope.getEvent());
        scMessage *message = dynamic_cast<scMessage *>(orgEnvelope->getEvent());
        assert(response != SC_NULL);
        assert(message != SC_NULL);
        matchTask->handleResponse(message, *response);
      } else {
        if (reqItem.getHandler() != SC_NULL)
          handleResponseByReqHandler(*orgEnvelope, envelope, reqItem.getHandler());
        else  
          handleUnknownResponse(envelope);
      }  
      notifyObserversResponseHandled(envelope, reqItem);
    } 
    catch(scError &excp) {
      notifyObserversResponseHandleError(envelope, reqItem, excp);
      throw;
    }
    catch(...) {
      notifyObserversResponseHandleError(envelope, reqItem);
      throw;
    }
  } else {
    notifyObserversResponseUnknownIdArrived(envelope);
    handleUnknownResponse(envelope);
  }      
}

void scScheduler::handleResponseByReqHandler(const scEnvelope &orgEnvelope,
  const scEnvelope &respEnvelope, scRequestHandler *handler)
{
  scMessage *message = dynamic_cast<scMessage *>(orgEnvelope.getEvent());
  scResponse *response = dynamic_cast<scResponse *>(respEnvelope.getEvent());
  assert(message != SC_NULL);
  assert(response != SC_NULL);
  assert(handler != SC_NULL);
  if (response->isError()) 
    handler->handleReqError(*message, *response);
  else    
    handler->handleReqResult(*message, *response);
}

void scScheduler::handleUnknownResponse(const scEnvelope &envelope)
{
#ifdef SC_LOG_ERRORS
   scResponse *response = dynamic_cast<scResponse *>(envelope.getEvent());
   int status = response->getStatus();
   int requestId = response->getRequestId();
   scString msg;

   msg = "Unknown response found, status: "+toString(status);
   msg += ", request ID: "+toString(requestId);
   
   if (response->getError().hasChild("text"))
     msg += ", error: "+response->getError()["text"].getAsString();
     
   if (response->getResult().hasChild("text"))
     msg += ", result: "+response->getResult()["text"].getAsString();

   Log::addError(msg); 
#endif   
}

bool scScheduler::cancelRequest(int requestId) 
{
  scRequestItem foundItem;
  return matchResponse(requestId, foundItem);
}

bool scScheduler::matchResponse(int requestId, scRequestItem &foundItem) 
{
  bool res = false;
  
  scRequestItemMapColnIterator p;

  p = m_waitingMessages.find(requestId);
  if(p != m_waitingMessages.end()) {
      res = true;
      foundItem = *p->second;
      if (foundItem.getEnvelope() != SC_NULL)
        notifyObserversMsgWaitEnd(foundItem.getEnvelope()->getEvent()->getRequestId());
      m_waitingMessages.erase(p);
  }    
  
  return res;  
}

bool scScheduler::matchResponse(const scEnvelope &envelopeResponse, scRequestItem &foundItem) 
{
  int requestId = envelopeResponse.getEventRef().getRequestId();
  return matchResponse(requestId, foundItem);
}

void scScheduler::checkTimeouts() 
{
  scRequestItem *foundItem;
  scEnvelope *renvelope;
  scRequestItemMapColnIterator inext;
  
  int requestId;
  uint timeout;
  scEnvelope *envelope;
  
  for(scRequestItemMapColnIterator i=m_waitingMessages.begin(); i!=m_waitingMessages.end(); )
  {
    inext = i;
    inext++;

    foundItem = i->second;
    if ((foundItem->getEnvelope() != SC_NULL) && (foundItem->getEnvelope()->getTimeout() != 0))
    {
      envelope  = foundItem->getEnvelope();
      timeout = envelope->getTimeout();
      if (is_cpu_time_elapsed_ms(foundItem->getStartTime(), timeout))
      {
         if (envelope->getEvent() != SC_NULL)
           requestId = envelope->getEvent()->getRequestId();
         else  
           requestId = SC_REQUEST_ID_NULL;
           
         renvelope = createErrorResponseFor(
           *envelope, 
           "Timeout for message ["+toString(requestId)+"]", 
           SC_RESP_STATUS_TIMEOUT);      
    
         postEnvelopeForThis(renvelope);        
         notifyObserversMsgWaitEnd(foundItem->getRequestId());
         m_waitingMessages.erase(i);
      }        
    }
    i = inext;
  } // for
}

scTaskIntf *scScheduler::findTask(const scMessageAddress &address) 
{
  scTaskIntf *res = SC_NULL;
  if (address.getTask().size())
  {
    for(scTaskColnIterator i=m_tasks.begin(); i!=m_tasks.end(); ++i){
      if (address.getTask() == i->getName())
      {
        res = &(*i);
        break;
      }  
    } // for
  } // if  
  return res; 
}

scTaskIntf *scScheduler::findTaskForMessage(const scString &command, const scDataNode &params)
{
  scTaskIntf *res = SC_NULL;
  for(scTaskColnIterator i=m_tasks.begin(); i!=m_tasks.end(); ++i){
    if (i->acceptsMessage(command, params))
    {
      res = &(*i);
      break;
    }  
  } // for
  return res; 
}

int scScheduler::dispatchResponseForHandlers(scEvent *eventMessage, scEvent *eventResponse)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  int hnd_res;
  scResponse *response;
  scMessage *message;
  scTaskIntf *newTask;

  message = dynamic_cast<scMessage *> (eventMessage);
  response = dynamic_cast<scResponse *> (eventResponse);

  for(scModuleListIterator i=m_modules.begin(); i!=m_modules.end(); ++i){
    hnd_res = (*i)->handleResponse(message, response);
    if (hnd_res == SC_MSG_STATUS_OK) 
    {
      res = hnd_res;
      break;
    }
    if (hnd_res == SC_MSG_STATUS_TASK_REQ) 
    { 
      newTask = (*i)->prepareTaskForResponse(response);
      if (newTask != SC_NULL) 
      {
        addTask(newTask);
        res = SC_MSG_STATUS_OK;
        break;
      }
    }
    
    if (hnd_res != SC_MSG_STATUS_PASS) 
    { // error
      res = hnd_res;
      break;
    }
  } 
  return res;    
}

void scScheduler::setName(const scString &a_name)
{
  m_name = a_name;
}

void scScheduler::registerSelf()
{
  scString newName;
  registerNodeAs(getRegistrationId(), getOwnAddress().getAsString(), newName);
}

scString scScheduler::getName() const
{
  return m_name;
}

uint scScheduler::getFeatures()
{
  return m_features;
}

void scScheduler::setFeatures(uint value)
{
  m_features = value;
}

bool scScheduler::isFeatureActive(scSchedulerFeature feature)
{
  return ((m_features & feature) != 0);
}

/// returns name under which this scheduler is registered at central registry
scString scScheduler::getRegistrationId() const
{
  if (m_registrationId.empty())
    //return m_name+"_"+toString(wxGetProcessId());
    return m_name+"_"+toString(getCurrentProcessId());
  else
    return m_registrationId;  
}

void scScheduler::setRegistrationId(const scString &value)
{
  m_registrationId = value;
}

scSchedulerStatus scScheduler::getStatus() const
{
  return m_status;
}  

void scScheduler::setStatus(scSchedulerStatus value)
{
  m_status = value;
}

void scScheduler::setLocalRegistry(scLocalNodeRegistry *registry)
{
  scMessageGateInproc *gate;

  m_localRegistry = registry;
  
  for(scMessageGateColnIterator i=m_inputGates.begin(); i!=m_inputGates.end(); ++i)
  {
    gate = dynamic_cast<scMessageGateInproc *>(&(*i));
    if (gate != SC_NULL)
      gate->setLocalRegistry(registry);
  }    
  
  for(scMessageGateColnIterator i=m_outputGates.begin(); i!=m_outputGates.end(); ++i)
  {
    gate = dynamic_cast<scMessageGateInproc *>(&(*i));
    if (gate != SC_NULL)
      gate->setLocalRegistry(registry);
  }      
}

bool scScheduler::registerNodeAs(const scString &source, const scString &target, 
  scString &newName,
  bool publicEntry, bool directMode, cpu_ticks shareTime, cpu_ticks endTime)
{
  scMessageAddress src, trg;
  scRegEntryFeatureMask features = 0;
  scString srcName = source;
  
  if (publicEntry)
     features += refPublic;
  if (directMode)
     features += refDirectMode;

  if (source.empty())
  {
    newName = m_registry.genRegistrationId(target);          
    // register as role
    src.setAsString(scString("@")+newName);
    srcName = newName;
  } else { 
    newName = "";
    src.setAsString(source);
  }  
  trg.setAsString(target);

  scNodeRegistryHandle entryHandle = 0;
  
  switch (src.getFormat()) 
  {
    case scMessageAddress::AdrFmtVPath:
      entryHandle = m_registry.registerNodeForPath(trg, src.getPath(), features);
      break;
    case scMessageAddress::AdrFmtRole:  
      entryHandle = m_registry.registerNodeForRole(trg, src.getRole(), features);
      break;
    case scMessageAddress::AdrFmtDefault:
      entryHandle = m_registry.registerNodeForName(trg, src.getNode(), features);
      break;
    case scMessageAddress::AdrFmtRaw:
      entryHandle = m_registry.registerNodeForName(trg, srcName, features);
      break;
    default:
      throw scError("Unknown address format: ["+source+"]");      
  } 

  if (shareTime > 0)
    m_registry.setEntryShareTime(entryHandle, shareTime);
  if (endTime > 0)
    m_registry.setEntryEndTime(entryHandle, endTime);  
      
  if (!newName.empty() && !isDirectoryNull())
  {
    // source = new-id as role
    // target = own-dir(dir-protocol) + new-id as role
    scMessageAddress dirAddr = getDirectoryAddr();
    scMessageAddress targetDir = getOwnAddress(dirAddr.getProtocol());
    targetDir.setTask(src.getAsString());
    registerNodeAtDirectory(src.getAsString(), targetDir.getAsString(), shareTime);
  }  
  return true;
}

bool scScheduler::hasNodeInRegistry(const scString &source)
{
  return m_registry.isRegistered(source);
}

void scScheduler::registerNodeAtDirectory(const scString &srcAddr, const scString &targetAddr, cpu_ticks shareTime)
{
  scString dirAddr = getDirectoryAddr();
  if (!dirAddr.empty()) {
    scDataNode newParams;
    newParams.addChild("source", new scDataNode(srcAddr));
    newParams.addChild("target", new scDataNode(targetAddr));
    newParams.addChild("public", new scDataNode("true"));
    newParams.addChild("direct_contact", new scDataNode("false"));
    if (shareTime > 0)
      newParams.addChild("share_time", new scDataNode(shareTime));

#ifdef DEBUG_LOG_MSGS
    Log::addDebug("sending reg req to dir: "+dirAddr+", src: "+srcAddr+", target: "+targetAddr);
#endif      
    postMessage(dirAddr, "core.reg_node", &newParams);  
  }
}

void scScheduler::registerNodeService(const scString &sourceKey, const scString &serviceName)
{
  m_registry.registerNodeService(sourceKey, serviceName);
}

void scScheduler::registerCommandMap(const scString &cmdFilter, const scString &targetName, int priority)
{
  m_commandMap->registerCommandMap(cmdFilter, targetName, priority);
}

bool scScheduler::isSameProtocol(const scString &protocol1, const scString &protocol2)
{
  bool inproc1, inproc2;
  inproc1 = (protocol1.empty() || (protocol1 == SC_PROTOCOL_INPROC));
  inproc2 = (protocol2.empty() || (protocol2 == SC_PROTOCOL_INPROC));
  return (inproc1 && inproc2) || (protocol1 == protocol2);
}

void scScheduler::getRegistryEntriesForRole(const scString &protocol, const scString &roleName, const scString &searchKey, 
  bool publicOnly, scDataNode &output)
{
  scDataNode resList;
    
  m_registry.getAddrListForRoleAndKey(roleName, searchKey, publicOnly, resList);

  scString addr;
  scMessageAddress addrStruct;

  scDataNode outBuffer;  
  scString address;
  std::auto_ptr<scDataNode> addrGuard;
  cpu_ticks shareTime;
  
  outBuffer.clear();  
  for(uint i=0, epos = resList.size(); i != epos; i++)
  { 
    address = resList.getElement(i).getString("address");
    shareTime = resList.getElement(i).getUInt64("share_time", 0);
    
    if (address == roleName)
    {// indirect address
      addrStruct = getOwnAddress(protocol);
      addrStruct.setTask(scString("@")+roleName);
      addr = addrStruct.getAsString();
    } else {      
      addrStruct.setAsString(address);
      if (protocol.empty() || isSameProtocol(addrStruct.getProtocol(), protocol))
        addr = address;
      else 
        addr = "";  
    }  
    if (!addr.empty() && !outBuffer.hasChild(addr))
    {
      addrGuard.reset(new scDataNode(addr));
      if (shareTime > 0)
        addrGuard->addChild("share_time", new scDataNode(shareTime));
      outBuffer.addChild(addrGuard.release());     
    }  
  }  
  // convert names to values
  output.clear();
  output.setAsArray(vt_datanode);
  for(int i=0, epos = outBuffer.size(); i != epos; i++) {
    addrGuard.reset(new scDataNode());
    addrGuard->addChild("address", new scDataNode(outBuffer.getElementName(i)));
    if (outBuffer.getElement(i).hasChild("share_time"))
      addrGuard->addChild("share_time", 
        new scDataNode(
          outBuffer.getElement(i).getUInt64("share_time")));
    output.addItem(*addrGuard);
  }  
}

void scScheduler::setDispatcher(const scString &address)
{
  m_dispatcher = address;
}

void scScheduler::setDirectoryAddr(const scString &address)
{
  m_directoryAddr = address;
}

scString scScheduler::getDirectoryAddr()
{
  return m_directoryAddr;
}

bool scScheduler::isDirectoryNull()
{
  return m_directoryAddr.empty();
}

void scScheduler::createNodes(const scString &a_className, int nodeCount, const scString &a_namePattern)
{
  Log::addDebug("Creating node: "+a_className);

  assert(m_localRegistry != SC_NULL);
  scString coreName = a_namePattern;
  
  if (coreName.empty())
    coreName = "A";

  if (nodeCount == 1)
  {
    scString newName = genNewNodeName(coreName);
    scScheduler *newNode;

    m_localRegistry->add(scNodeFactory::createNode(a_className, newName));        
    newNode = dynamic_cast<scScheduler *>(m_localRegistry->find(newName));
    newNode->setLocalRegistry(m_localRegistry);
    newNode->init();    
  } 
  else if (nodeCount > 1)
  {
    scString newName;
    scScheduler *newNode;
    
    for(int i=1; i <= nodeCount; i++)
    {
      newName = genNewNodeName(coreName+toString(i));
      m_localRegistry->add(scNodeFactory::createNode(a_className, newName));        
      newNode = dynamic_cast<scScheduler *>(m_localRegistry->find(newName));
      newNode->setLocalRegistry(m_localRegistry);
      newNode->init();
    }  
  }
}

scString scScheduler::genNewNodeName(const scString &a_coreName)
{
  scString res = a_coreName;
  if (m_localRegistry != SC_NULL) 
  {
     if (m_localRegistry->find(a_coreName) != SC_NULL)
     {
       int cnt = 1;
       while(1) { // breaks on int(cnt) overflow
         res = a_coreName + toString(cnt);
         if (m_localRegistry->find(a_coreName) == SC_NULL)
           break;
         ++cnt;  
       } // while
     } // core name found
  } // local registry assigned 
  
  return res;
}

void scScheduler::getStats(int &taskCnt, int &moduleCnt, int &gateCnt)
{
  taskCnt = moduleCnt = gateCnt = -1;
  taskCnt = m_tasks.size();
  moduleCnt = m_modules.size();
  gateCnt = m_inputGates.size() + m_outputGates.size();
}

// Resolve destination address and send message to this address
// if not found - try to forward
// if forward fails - error
// - send resolving requests: core.advertise to directory
// - create new handler - for posting to correct address + save the requested handler
// - wait for result
// - if result found 
//   - add entry to local dir
//   - use normal postMessage with corrected address
// - otherwise:
//   - if dispacher configured - use "forward"  
//   - otherwise - error
bool scSchedulerImpl::resolveDestForMessage(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandler *handler)
{
  std::auto_ptr<scResolveHandler> resolveHandler(new scResolveHandler());

  resolveHandler->setScheduler(this);
  resolveHandler->setOrgAddress(address);
  resolveHandler->setOrgCommand(command);
  resolveHandler->setOrgParams(params);
  resolveHandler->setOrgRequestId(requestId);
  resolveHandler->setOrgHandler(handler);

  bool res = false;
  if (m_directoryAddr.length()) {
    scDataNode newParams;
    scMessageAddress addr(address);
    newParams.addChild("role_name", new scDataNode(addr.getRole()));

#ifdef DEBUG_LOG_MSGS
    Log::addDebug("Forwarding message to: "+address+", command: "+command);
#endif      
    postMessage(m_directoryAddr, "core.advertise", &newParams, getNextRequestId(), resolveHandler.release());  
    res = true;
  } else {
    // make sure handler is referenced - just in case (not required)
    scRequestHandlerTransporter transporter(handler);
  }
  return res;
}      

void scScheduler::notifyObserversEnvArrived(const scEnvelope &envelope)
{
  if (isFeatureActive(sfLogMessages))
  {
    scString envText;
    checkEnvelopeSerializer()->convToString(envelope, envText);
    int reqId;
    if (envelope.getEvent() != SC_NULL)
        reqId = envelope.getEvent()->getRequestId();
    else
        reqId = -1;
    Log::addInfo(scString("Message arrived [")+toString(reqId)+", contents: ["+envText+scString("]"));
  }
}

//void scScheduler::notifyObserversMsgArrived(const scMessage &message)
//{
//  if (isFeatureActive(sfLogMessages))
//  {
//    scEnvelope envelope;
//    envelope.setEvent(new scMessage(message));
//    scString envText;
//    checkEnvelopeSerializer()->convToString(envelope, envText);
//    Log::addInfo(scString("Message arrived [")+toString(message.getRequestId())+", contents: ["+envText+scString("]"));
//  }
//}

void scScheduler::notifyObserversMsgHandled(const scMessage &message, const scResponse &response, int resultStatus)
{
  if (isFeatureActive(sfLogMessages))
  {
    Log::addInfo(scString("Message handled [")+toString(message.getRequestId())+scString("], status = ")+toString(resultStatus));
  }
}

void scScheduler::notifyObserversMsgHandleError(const scMessage &message, const scResponse &response, const std::exception& excp)
{
  if (isFeatureActive(sfLogMessages))
  {
    Log::addInfo(scString("Message handle error [")+toString(message.getRequestId())+scString("], what = ")+scString(excp.what()));
  }
}

void scScheduler::notifyObserversMsgReadyForSend(const scEnvelope &envelope, const scMessageGate &gate)
{
  if (isFeatureActive(sfLogMessages))
  {
    scString msg;
    if (envelope.getEvent()->isResponse())
      msg = "Response";
    else
      msg = "Message";  
    Log::addInfo(msg+scString(" ready for send [")+toString(envelope.getEvent()->getRequestId())+scString("]"));
    scString envText;
    checkEnvelopeSerializer()->convToString(envelope, envText);
    Log::addInfo(msg+scString(" contents: [")+envText+scString("]"));
  }

  if (!envelope.getEvent()->isResponse())
  {
    dtpString command = dynamic_cast<scMessage *>(envelope.getEvent())->getCommand();
    scMessageTrace::addTrace("req_rdy",envelope.getSender().getAsString(), envelope.getReceiver().getAsString(), envelope.getEvent()->getRequestId(), command);
  }
}

void scScheduler::notifyObserversMsgWaitStarted(const scEnvelope &envelope, uint requestId)
{
}

void scScheduler::notifyObserversMsgWaitEnd(uint requestId)
{
}

void scScheduler::notifyObserversResponseArrived(const scEnvelope &envelope, const scRequestItem &reqItem)
{
  cpu_ticks procTime = calc_cpu_time_delay(reqItem.getStartTime(), cpu_time_ms());

#ifdef GRD_TRACE_SCHEDULER_TIME
  perf::Timer::inc("msg-proc-scheduler", procTime);
#endif

#ifdef GRD_TRACE_SCHEDULER_TIME_FOR_CMD
  scString cmdTime = dynamic_cast<scMessage *>(const_cast<scRequestItem &>(reqItem).getEnvelope()->getEvent())->getCommand();
  perf::Timer::inc("msg-proc-scheduler-"+cmdTime, procTime);
#endif

#ifdef GRD_TRACE_SCHEDULER_COUNTER
  perf::Counter::inc("msg-proc-scheduler", 1);
#endif

#ifdef GRD_TRACE_SCHEDULER_COUNTER_FOR_CMD
  scString cmdCnt = dynamic_cast<scMessage *>(const_cast<scRequestItem &>(reqItem).getEnvelope()->getEvent())->getCommand();
  perf::Counter::inc("msg-proc-scheduler-"+cmdCnt, 1);
#endif

  if (isFeatureActive(sfLogProcTime))
  {
    uint requestId = reqItem.getRequestId();
    Log::addInfo(scString("Request [")+toString(requestId)+scString("] handled in [")+toString(procTime)+"ms / "+
      timeToIsoStr(msecsToDateTime(procTime))+scString("]"));
  }

  if (isFeatureActive(sfLogMessages))
  {
    scString cmd = dynamic_cast<scMessage *>(const_cast<scRequestItem &>(reqItem).getEnvelope()->getEvent())->getCommand();
    Log::addInfo(scString("Response arrived for [")+toString(envelope.getEvent()->getRequestId())+scString("], cmd: ")+cmd);
    scString envText;
    checkEnvelopeSerializer()->convToString(envelope, envText);
    Log::addInfo(scString("Response contents: [")+envText+scString("]"));
  }
}

void scScheduler::notifyObserversResponseUnknownIdArrived(const scEnvelope &envelope)
{
  if (isFeatureActive(sfLogMessages))
  {
    Log::addInfo(scString("Unknown response received [")+toString(envelope.getEvent()->getRequestId())+scString("]"));
  }
}

void scScheduler::notifyObserversResponseHandled(const scEnvelope &envelope, const scRequestItem &reqItem)
{
  if (isFeatureActive(sfLogMessages))
  {
    Log::addInfo(scString("Response handled OK for [")+toString(reqItem.getRequestId())+scString("]"));
  }
}

void scScheduler::notifyObserversResponseHandleError(const scEnvelope &envelope, const scRequestItem &reqItem, const scError &excp)
{
  if (isFeatureActive(sfLogMessages))
  {
    Log::addInfo(scString("Response handle error for [")+toString(reqItem.getRequestId())+scString("], what: ")+excp.what()+", details: "+const_cast<scError &>(excp).getDetails());
  }
}

void scScheduler::notifyObserversResponseHandleError(const scEnvelope &envelope, const scRequestItem &reqItem)
{
  if (isFeatureActive(sfLogMessages))
  {
    Log::addInfo(scString("Response handle error for [")+toString(reqItem.getRequestId())+scString("], what: (?)"));
  }
}

scEnvelopeSerializerBase *scScheduler::checkEnvelopeSerializer()
{
  if (m_envelopeSerializer.get() == SC_NULL)
    m_envelopeSerializer.reset(new scEnvSerializerJsonYajl);
    
  return m_envelopeSerializer.get();   
}
