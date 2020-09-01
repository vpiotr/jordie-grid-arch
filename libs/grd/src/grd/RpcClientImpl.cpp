/////////////////////////////////////////////////////////////////////////////
// Name:        RpcClientImpl.cpp
// Project:     grdLib
// Purpose:     Implementation of work queue client interface.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RpcClientImpl.h"
#include "sc/log.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

//-------------------------------------------------------------------------------
// Private classes
//-------------------------------------------------------------------------------
/// Implementation class of RPC request 
class scRpcRequestImpl: public scRpcRequest {
public:
// construct
  scRpcRequestImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget);
  virtual ~scRpcRequestImpl();
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
  scRpcRequestImpl() {}
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

/// Implementation class of RPC request group
class scRpcRequestGroupImpl: public scRpcRequestGroup {
public:
// construct
  scRpcRequestGroupImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget);
  virtual ~scRpcRequestGroupImpl() {}
  virtual void waitFor();  
  virtual uint waitForAny(); // returns first ready request index that was not rdy before call
  virtual scRpcRequest *newRequest();
protected:  
  scRpcRequestGroupImpl() {}
  void yieldWait();
  uint indexOfReadyRequest();
protected:   
  scScheduler *m_scheduler;  
  scSignal *m_waitSignal;
  scString m_workTarget;
};

/// Local request handler for passing response back to request object
class scRpcCliRequestHandler: public scRequestHandler {
public:
  scRpcCliRequestHandler(scRpcRequestImpl *owner);
  virtual ~scRpcCliRequestHandler();
  virtual void handleCommError(const scString &errorText, RequestPhase phase);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
protected:
  scRpcRequestImpl *m_owner;  
};

//-------------------------------------------------------------------------------
// scRpcRequestImpl
//-------------------------------------------------------------------------------
scRpcRequestImpl::scRpcRequestImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget): 
  scRpcRequest(), m_scheduler(scheduler), m_waitSignal(waitSignal), m_requestId(SC_REQUEST_ID_NULL), m_statusCode(0), m_workTarget(workTarget)
{
}

scRpcRequestImpl::~scRpcRequestImpl()
{
}

// properties
bool scRpcRequestImpl::isResultReady() const
{
  return (m_result.get() != SC_NULL);
}

bool scRpcRequestImpl::isResultOk() const
{
  bool res = false;
  if (isResultReady())
    res = (getStatusCode() == SC_MSG_STATUS_OK);
  return res;  
}

bool scRpcRequestImpl::getResult(scDataNode &output) const
{
  bool res = (m_result.get() != SC_NULL);
  if (res)
    output = *m_result;
  return res;    
}

bool scRpcRequestImpl::getStatus(scDataNode &output) const
{
  bool res = (m_status.get() != SC_NULL);
  if (res)
    output = *m_status;
  return res;  
}

int scRpcRequestImpl::getStatusCode() const
{
  if (isResultReady()) {
    return m_statusCode;
  }  
  else {
    return SC_MSG_STATUS_WAITING;  
  }  
}

void scRpcRequestImpl::setStatus(int statusCode, const scDataNode &statusData)
{
  m_statusCode = statusCode;
  m_status.reset(new scDataNode(statusData));
}

bool scRpcRequestImpl::getHandlerName(scString &output) const
{
  output = m_handlerName;
  return !(output.empty());
}

void scRpcRequestImpl::setHandlerName(const scString &value)
{
  m_handlerName = value;
}

void scRpcRequestImpl::setResult(const scDataNode &resultData)
{
  m_result.reset(new scDataNode(resultData));
  if (resultData.hasChild("_worker_id"))
    setHandlerName(resultData.getString("_worker_id"));
}

void scRpcRequestImpl::setCommand(const scString &command)
{
  m_command = command;
}

const scString &scRpcRequestImpl::getCommand() const
{
  return m_command;
}

void scRpcRequestImpl::setParams(const scDataNode &params)
{
  m_params = params;
}

bool scRpcRequestImpl::getParams(scDataNode &output) const
{
  output = m_params;
  return true;
}

const scString scRpcRequestImpl::getWorkTargetAddr()
{
  return m_workTarget;
}

scScheduler *scRpcRequestImpl::getScheduler()
{
  return m_scheduler;
}

// run
void scRpcRequestImpl::executeAsync()
{
  scDataNode params;
  if (getParams(params)) {
    std::auto_ptr<scRequestHandler> newHandlerGuard(new scRpcCliRequestHandler(this));
    scString addr(getWorkTargetAddr());
    m_requestId = getScheduler()->getNextRequestId();
    getScheduler()->postMessage(addr, getCommand(), &params, m_requestId, newHandlerGuard.release());
  }  
}

void scRpcRequestImpl::notify()
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
void scRpcRequestImpl::cancel()
{
  if (m_requestId != SC_REQUEST_ID_NULL)
    getScheduler()->cancelRequest(m_requestId);
  setStatus(SC_MSG_STATUS_USR_ABORT, scDataNode());  
}

void scRpcRequestImpl::waitFor()
{
  while(!isResultReady())
    yieldWait();
}

void scRpcRequestImpl::throwLastError()
{
  scDataNode status;
  getStatus(status);
  throw scError("Request error.").
    addDetails(
      scDataNode().
        addChild("command", scDataNode(m_command)).
        addChild("params", scDataNode(m_params)).
        addChild("statusCode", scDataNode(getStatusCode())).
        addChild("status", scDataNode(status)));
}

void scRpcRequestImpl::checkStatus()
{
  if (!isResultReady())
    throw scError("Result not ready.").
      addDetails(
        scDataNode().
          addChild("command", m_command).
          addChild("params", scDataNode(m_params)));
  if (!isResultOk())
    throwLastError();
}

void scRpcRequestImpl::yieldWait()
{
  if (m_waitSignal != SC_NULL)
    m_waitSignal->execute();
}

//-------------------------------------------------------------------------------
// scRpcRequestGroupImpl
//-------------------------------------------------------------------------------
scRpcRequestGroupImpl::scRpcRequestGroupImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget): 
  scRpcRequestGroup(), m_scheduler(scheduler), m_waitSignal(waitSignal), m_workTarget(workTarget)
{
}

void scRpcRequestGroupImpl::waitFor()
{
  while(!isResultReady())
    yieldWait();
}

uint scRpcRequestGroupImpl::waitForAny()
{
  while(!isAnyResultReady())
    yieldWait();
  
  return indexOfReadyRequest(); 
}

uint scRpcRequestGroupImpl::indexOfReadyRequest()
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

void scRpcRequestGroupImpl::yieldWait()
{
  if (m_waitSignal != SC_NULL)
    m_waitSignal->execute();
}

scRpcRequest *scRpcRequestGroupImpl::newRequest()
{
  return new scRpcRequestImpl(m_scheduler, m_waitSignal, m_workTarget);
}

// ----------------------------------------------------------------------------
// scRpcCliRequestHandler
// ----------------------------------------------------------------------------
scRpcCliRequestHandler::scRpcCliRequestHandler(scRpcRequestImpl *owner): m_owner(owner)
{
  assert(m_owner != SC_NULL);
}

scRpcCliRequestHandler::~scRpcCliRequestHandler()
{
}

void scRpcCliRequestHandler::handleCommError(const scString &errorText, RequestPhase phase)
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
  scLog::addError("scRpcCliRequestHandler::handleCommError ["+errorText+"]");
#endif  
}

void scRpcCliRequestHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
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

  scLog::addText("scRpcCliRequestHandler::handleReqResult - ["+text+"]");
#endif  
}

void scRpcCliRequestHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
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

  scLog::addText("scRpcCliRequestHandler::handleReqError - ["+text+"]");
#endif  
}

//-------------------------------------------------------------------------------
// scRpcServerProxyImpl
//-------------------------------------------------------------------------------
scRpcServerProxyImpl::scRpcServerProxyImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget): 
  m_scheduler(scheduler), m_waitSignal(waitSignal), m_workTarget(workTarget)
{
}

scRpcRequest *scRpcServerProxyImpl::newRequest()
{
  return new scRpcRequestImpl(m_scheduler, m_waitSignal, m_workTarget);
}

scRpcRequestGroup *scRpcServerProxyImpl::newGroup()
{
  return new scRpcRequestGroupImpl(m_scheduler, m_waitSignal, m_workTarget);
}

