/////////////////////////////////////////////////////////////////////////////
// Name:        JobWorkerTask.cpp
// Project:     scLib
// Purpose:     Base class for worker tasks.
// Author:      Piotr Likus
// Modified by:
// Created:     02/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////
#include "grd/JobWorkerTask.h"

scJobWorkerTask::scJobWorkerTask(scMessage *message)
{
  m_outputSyncPack.reset(new scMessagePack());
  m_ended = m_started = false;
  m_syncStage = jwtssNone;
  m_syncAction = jwtsoNull;
  m_stateLoaded = false;
  m_msgAddr = m_logAddr = "";
  processTaskParams(message->getParams());
}

scJobWorkerTask::~scJobWorkerTask()
{
}

scDataNode &scJobWorkerTask::getJobParams()
{
  return m_jobParams;
}

scString scJobWorkerTask::getReturnAddr()
{
  return m_returnAddr;
}

bool scJobWorkerTask::acceptsMessage(const scString &command, const scDataNode &params)
{
  if ((command == "job_worker.cancel_work") && params.hasChild("job_id") && (params.getUInt("job_id", 0) == m_jobId))
    return true;
  else 
    return scTask::acceptsMessage(command, params);  
}  

// process parameters: job_id,lock_id,trans_id,command,log_level,msg_level,priority,job_params
void scJobWorkerTask::processTaskParams(const scDataNode &params)
{
  m_returnAddr = params.getString("return_addr");
  m_msgAddr = params.getString("msg_addr", "");  
  m_logAddr = params.getString("log_addr", "");  
  m_jobParams = params["job_params"];
  m_jobId = params.getUInt64("job_id");
  m_lockId = params.getUInt("lock_id");
  m_transId = params.getUInt64("trans_id", 0);
  m_logLevel = params.getUInt("log_level", jmlError);
  m_msgLevel = params.getUInt("msg_level", jmlError);
  uint priority = params.getUInt("priority", 0);
  if (priority == 0)
    priority = JMM_DEF_PRIORITY;
  setPriority(priority);
}

void scJobWorkerTask::processJobParams(const scDataNode &params)
{ // empty here  
}

bool scJobWorkerTask::needsRun()
{
  return scTask::needsRun() || inSyncAction();
}

int scJobWorkerTask::runStarting()
{ 
  if (!m_started)
    startWork();
  return scTask::runStarting();
}

int scJobWorkerTask::intRun()
{
  int res = 0;
  
  try {
    if (checkSyncAction())
      res = 1;
    else 
      res = runStep();
    }
  catch(std::runtime_error excp) {
    if (!handleWorkUnitError(excp))
      throw;
  }  
  return res;
}

// returns <true> if handled
bool scJobWorkerTask::handleWorkUnitError(std::runtime_error &excp)
{
#ifdef JWT_LOG_ERRORS
  scLog::logError(scString("exception: ")+excp.what());
#endif    
  resetSyncActions();
  endWork(JEC_WU_EXCEPTION, scString("exception: ")+excp.what()); 
  return true;
}

void scJobWorkerTask::startWork()
{
  processJobParams(m_jobParams);
  if (stateRequired()) 
    loadState();
  intStartWork();
}

int scJobWorkerTask::runStep()
{ // empty here
  return 0;
}

bool scJobWorkerTask::inTransaction()
{
  return (m_transId > 0);
}

void scJobWorkerTask::commitWork(bool chained)
{
  scDataNode params;
  params.addChild("chained", new scDataNode(chained));  
  params.addChild("command", new scDataNode("commit"));  
  syncAction(jwtsoCommit, &params, true, true);
}

void scJobWorkerTask::rollbackWork(bool chained)
{
  scDataNode params;
  params.addChild("chained", new scDataNode(chained));  
  params.addChild("command", new scDataNode("rollback"));  
  syncAction(jwtsoRollback, &params, true, true);
}

/// call it only on real end
void scJobWorkerTask::endWork(int status, const scString &msg)
{
  scDataNode context;
  context.addChild("status", new scDataNode(status));
  context.addChild("msg", new scDataNode(msg));
  
  syncAction(jwtsoEndWork, &context, true, true);
}

void scJobWorkerTask::intStartWork()
{ // empty
}

void scJobWorkerTask::intEndWork(int status, const scString &msg)
{ // empty
}

void scJobWorkerTask::postWorkEnded(int status, const scString &msg)
{ 
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("lock_id", new scDataNode(m_lockId));
  if (m_transId > 0)
    params.addChild("trans_id", new scDataNode(m_transId));
  params.addChild("success", new scDataNode((status == SC_MSG_STATUS_OK)));
  params.addChild("end_status", new scDataNode(status));
  if (status != SC_MSG_STATUS_OK)
    params.addChild("error_msg", new scDataNode(msg));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.ended");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
  //was: getScheduler()->postEnvelope(envelopeGuard.release());
}

void scJobWorkerTask::postWithSync(scEnvelope *envelope)
{
  m_outputSyncPack->addEnvelope(envelope);
  m_outputSyncPack->post(getScheduler());
}

bool scJobWorkerTask::isAllSynced()
{
  return m_outputSyncPack->isAllHandled();
}

void scJobWorkerTask::beforeSyncAction(int action, const scDataNode &context)
{ // empty
}

void scJobWorkerTask::execSyncAction(int action, const scDataNode &context)
{
  switch (action) {
    case jwtsoCommit:
      doCommitWork(context);
      break;  
    case jwtsoRollback:
      doRollbackWork(context);
      break;  
    case jwtsoSyncPoint:
      // do nothing
      break;  
    case jwtsoLoadState:  
      doLoadState(context);
      break;
    case jwtsoEndWork:
      doEndWork(context);
      break;
    default:
      throw scError("Uknown sync action: "+toString(action));
  }      
}

void scJobWorkerTask::afterSyncAction(int action, const scDataNode &context)
{ 
  switch (action) {
    case jwtsoCommit:
    case jwtsoRollback:
      doAfterTransWork(context);
      break;  
    default:
      ; //do nothing      
  }      
}

void scJobWorkerTask::doEndWork(const scDataNode &context)
{
  if (!m_ended)
  {          
    m_ended = true;
    int status = context.getInt("status");
    scString msg = context.getString("msg");
    intEndWork(status, msg);
    postWorkEnded(status, msg);    
    requestStop();
  }   
}

void scJobWorkerTask::syncPoint()
{
  syncAction(jwtsoSyncPoint, SC_NULL, true, false);
}

void scJobWorkerTask::resetSyncActions()
{
  m_syncStage = jwtssNone;
  m_syncContext.clear();
}

void scJobWorkerTask::syncAction(int action, const scDataNode *context, bool syncBefore, bool syncAfter)
{
  if (m_syncStage != jwtssNone) 
    throw scError("Already in sync action, old: "+toString(m_syncAction)+", new: "+toString(action));

  if (context != SC_NULL)
    m_syncContext = *context;
  else  
    m_syncContext.clear();
      
  m_syncAction = action;
  m_syncBefore = syncBefore; 
  m_syncAfter = syncAfter;
  if (syncBefore) 
    m_syncStage = jwtssBefore; 
  else  
    m_syncStage = jwtssExec; 
}

bool scJobWorkerTask::inSyncAction()
{
  return (m_syncStage != jwtssNone);
}

// returns <true> if in the middle of sync action, performs sync handling if <true>
bool scJobWorkerTask::checkSyncAction()
{
  bool res = inSyncAction();
  if (res) {    
    if (m_syncStage == jwtssBefore) {
      if ((!m_syncBefore) || isAllSynced()) {
        m_syncStage = jwtssExec;
        beforeSyncAction(m_syncAction, m_syncContext);
      }         
    }

    if (m_syncStage == jwtssExec) {
      m_syncStage = jwtssAfter;
      execSyncAction(m_syncAction, m_syncContext);
    }

    if (m_syncStage == jwtssAfter) {
      if ((!m_syncAfter) || isAllSynced()) {
        m_syncStage = jwtssNone;
        scDataNode oldContext = m_syncContext;
        afterSyncAction(m_syncAction, oldContext);
        if (isAllSynced()) 
          m_outputSyncPack->clear();
      }         
    }

    res = inSyncAction();
  }      
  
  return res;
}

void scJobWorkerTask::setVars(const scDataNode &vars)
{
  if (!m_stateLoaded && stateRequired())
    throw scError("State not loaded before set");
    
  setVarsLocal(vars);
  setVarsRemote(vars);
}
  
void scJobWorkerTask::setVarsLocal(const scDataNode &vars)
{
  scString name;
  for(int i=0, epos = vars.size(); i != epos; i++)
  {      
    vars.getElementName(i, name);
    if (m_stateVars.hasChild(name)) {
      m_stateVars[name] = vars[i];
    } else {
      m_stateVars.addChild(vars.cloneElement(i));
    }  
  }
}

void scJobWorkerTask::setVarsRemote(const scDataNode &vars)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("lock_id", new scDataNode(m_lockId));
  if (m_transId > 0)
    params.addChild("trans_id", new scDataNode(m_transId));
  params.addChild("vars", new scDataNode(vars));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.set_vars");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

void scJobWorkerTask::getVars(scDataNode &output)
{
  if (!m_stateLoaded && stateRequired())
    throw scError("State not loaded");
    
  if (output.empty())
  { // get all
    output = m_stateVars;
  } else {      
    scString name;
    for(int i=0, epos = output.size(); i != epos; i++)
    {      
      output.getElementName(i, name);
      if (m_stateVars.hasChild(name))
        output[i] = m_stateVars[name];
      else
        output[i].setAsNull();  
    }
  }
}

void scJobWorkerTask::loadState()
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.get_state");
  messageGuard->setParams(params);
  messageGuard->setRequestId(getScheduler()->getNextRequestId());
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());

  syncAction(jwtsoLoadState, SC_NULL, true, false);
}

void scJobWorkerTask::doLoadState(const scDataNode &context)
{
  scDataNode fullResult, element;
    
  m_stateVars.clear();
  m_outputSyncPack->getFullResult(fullResult);
  
  for(int i=0, epos = fullResult.size(); i != epos; i++)
  {
    fullResult.getElement(i, element);
       
    for(int j=0, eposj = element.size(); j != eposj; j++)
    {
      m_stateVars.addChild(element.cloneElement(j));      
    }  
  }  

  m_stateLoaded = true;
}

bool scJobWorkerTask::stateRequired()
{
  return true;
}

void scJobWorkerTask::setVar(const scString &name, const scDataNode &value)
{
  scDataNode varList;
  varList.addChild(name, new scDataNode(value));
  setVars(varList);
}

void scJobWorkerTask::setVar(const scString &name, const scString &value)
{
  scDataNode varList;
  varList.addChild(name, new scDataNode(value));
  setVars(varList);
}

void scJobWorkerTask::setVarAsLongText(const scString &name, const scString &value)
{
  uint maxPartSize = JOB_MAX_STATE_VAR_LEN-1;
  uint parts = 1+(value.length() / maxPartSize);

  setVar(name+"_size", toString(parts));
  if (parts <= 1) { 
    setVar(name+"1", value);
  }  
  else {
    scString partText;
    uint leftLen = value.length();    
    uint partLen, partNo, offset;
    partNo = 1;
    while(leftLen > 0) {
      partText = value.substr(offset, maxPartSize);
      setVar(name+toString(partNo), partText);
      leftLen -= partText.length();
      partNo++;
      offset+= partText.length();
    }
  }  
}

void scJobWorkerTask::getVar(const scString &name, const scString &defValue, scString &output)
{
  scDataNode varList;
  varList.addChild(new scDataNode(name));
  getVars(varList);
  if (varList[0].isNull())
    varList.setString(0, defValue);
  output = varList.getString(0);  
}

scString scJobWorkerTask::getVar(const scString &name, const scString &defValue)
{
  scString res;
  getVar(name, defValue, res);
  return res;
}

void scJobWorkerTask::getVar(const scString &name, const scDataNode &defValue, scDataNode &output)
{
  scDataNode varList;
  varList.addChild(new scDataNode(name));
  getVars(varList);
  if (varList[0].isNull())
    varList[0] = defValue;
  output = varList[0];  
}

scDataNode scJobWorkerTask::getVar(const scString &name, const scDataNode &defValue)
{
  scDataNode res;
  getVar(name, defValue, res);
  return res;
}

void scJobWorkerTask::getVarAsLongText(const scString &name, const scString &defValue, scString &output)
{
  scString defSizeTxt("0");
  scString txtSize = getVar(name+"_size", defSizeTxt);
  scString partText;
  uint vSize = stringToUInt(txtSize);
  uint partNo = 1;
  output.clear();
  while (partNo <= vSize) {
    getVar(name+toString(partNo), "", partText);
    if (!partText.empty()) {
      output += partText;
      partNo++;
    } 
    else 
      break;
  }
}

scString scJobWorkerTask::getVarAsLongText(const scString &name, const scString &defValue)
{
  scString res;
  getVarAsLongText(name, defValue, res);
  return res;
}

void scJobWorkerTask::doCommitWork(const scDataNode &context)
{
  if (m_transId == 0)
    throw scError("No active transaction for commit");

  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("trans_id", new scDataNode(m_transId));
  params.addChild("lock_id", new scDataNode(m_lockId));
  params.addChild("chained", new scDataNode(context.getBool("chained")));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.commit");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

void scJobWorkerTask::doAfterTransWork(const scDataNode &context)
{
  if (context.getBool("chained")) {
    scDataNode msgResult, element;  
    
    if (!m_outputSyncPack->isResultOK())
      if (!handleTransActionFailed(context.getString("command","")))
        return;
      
    m_outputSyncPack->getFullResult(msgResult);    
    for(int i=0,epos=msgResult.size(); i != epos; i++)
    {
      msgResult.getElement(i, element);
      if (element.hasChild("trans_id")) {
        m_transId = element.getUInt64("trans_id");
        break;
      }
    }
  }    
}

void scJobWorkerTask::doRollbackWork(const scDataNode &context)
{
  if (m_transId == 0)
    throw scError("No active transaction for rollback");

  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("trans_id", new scDataNode(m_transId));
  params.addChild("lock_id", new scDataNode(m_lockId));
  params.addChild("chained", new scDataNode(context.getBool("chained")));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.rollback");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

// list of values: name, type, path
void scJobWorkerTask::allocResource(const scDataNode &resList)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("trans_id", new scDataNode(m_transId));
  
  params.addChild("list", new scDataNode());
  params["list"].copyValueFrom(resList);
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.alloc_res");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

void scJobWorkerTask::allocResource(const scString &name, const scString &resType, const scString &resPath)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("trans_id", new scDataNode(m_transId));
  
  params.addChild("name", new scDataNode(name));
  params.addChild("type", new scDataNode(resType));
  params.addChild("path", new scDataNode(resPath));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.alloc_res");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

// list of values: name
void scJobWorkerTask::deallocResource(const scDataNode &resList)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("trans_id", new scDataNode(m_transId));
  
  params.addChild("list", new scDataNode());
  params["list"].copyValueFrom(resList);
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.dealloc_res");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

void scJobWorkerTask::deallocResource(const scString &name)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_returnAddr));

  scDataNode params;
  params.addChild("job_id", new scDataNode(m_jobId));
  params.addChild("trans_id", new scDataNode(m_transId));
  
  params.addChild("name", new scDataNode(name));
  
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job.dealloc_res");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

// return <true> if program can continue
bool scJobWorkerTask::handleTransActionFailed(const scString &command)
{
  bool res = false;
  endWork(JEC_TRANS_ERROR, "Transaction failed on command: "+command);  
  return res;
}

void scJobWorkerTask::logText(const scString &a_text, scJobLogType severity, uint msgCode)
{
  if ((m_logLevel & uint(severity)) == 0) 
    return;
    
  scDataNode params;
  if (severity != jltInfo) {
    params.addChild("severity", new scDataNode(static_cast<uint>(severity)));    
  }
  if (msgCode != 0) {
    params.addChild("code", new scDataNode(msgCode));
  }
  params.addChild("text", new scDataNode(a_text));    
  params.addChild("job_id", new scDataNode(m_jobId));    
  postJobMessage("job.log_text", params, m_logAddr);
}

void scJobWorkerTask::displayText(const scString &a_text, scJobLogType severity)
{
  if ((m_msgLevel & uint(severity)) == 0)
    return;
    
  scDataNode params;
  params.addChild("text", new scDataNode(a_text));
  postJobMessage("gui.echo", params, m_msgAddr);
}

void scJobWorkerTask::postJobMessage(const scString &command, const scDataNode &params, const scString &address)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  scString useAddress = address;
  if (useAddress.empty())
    useAddress = m_returnAddr;
  envelopeGuard->setReceiver(scMessageAddress(useAddress));
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand(command);
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());
}

