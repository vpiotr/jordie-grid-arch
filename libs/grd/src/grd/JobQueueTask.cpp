/////////////////////////////////////////////////////////////////////////////
// Name:        JobQueueTask.cpp
// Project:     scLib
// Purpose:     Implementation of job queue manager task.
// Author:      Piotr Likus
// Modified by:
// Created:     29/12/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "dtp/string.h"
#include "dtp/date.h"

#include "sc/utils.h"
#include "sc/log.h"

#include "grd/JobQueueTask.h"

using namespace dtp;

// ----------------------------------------------------------------------------
// scJobSubmitHandler
// ----------------------------------------------------------------------------
class scJobSubmitHandler: public scRequestHandler {
public:
  scJobSubmitHandler(scJobQueueTask *sender, ulong64 jobId, uint lockId): scRequestHandler() 
    {m_sender = sender; m_jobId = jobId; m_lockId = lockId;};
  virtual ~scJobSubmitHandler() {};
  virtual void handleCommError(const scString &errorText, RequestPhase phase);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
protected:
  scJobQueueTask *m_sender;  
  ulong64 m_jobId;
  uint m_lockId;
};

void scJobSubmitHandler::handleCommError(const scString &errorText, RequestPhase phase)
{
  m_sender->handleSubmitError(m_jobId, errorText, m_lockId);
}

void scJobSubmitHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  scString workerAddr = a_response.getResult().getString("worker_addr", "");  
  m_sender->handleSubmitSuccess(m_jobId, workerAddr, m_lockId);
}

void scJobSubmitHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  scString msg = toString(a_response.getStatus())+": "+a_response.getError().getString("text", "");
  m_sender->handleSubmitError(m_jobId, msg, m_lockId);
}

// ----------------------------------------------------------------------------
// scJobQueueTask
// ----------------------------------------------------------------------------
scJobQueueTask::scJobQueueTask(const scString &queueName, const scString &targetAddr, const scString &returnAddr, 
  scDbBase *db, const scString &safeRootList, ulong64 purgeInterval, ulong64 purgeCheckInterval)
{
  m_queueName = queueName;
  m_targetAddr = targetAddr;
  m_returnAddr = returnAddr;
  m_db = db;
  m_closing = false;
  
  m_safeRootList = scDataNode::explode(",", strToUpper(safeRootList));
  m_lastTimeoutCheck = dateTimeToMSecs(currentDateTime());
  m_timeoutDelay = JQT_DEF_TIMEOUT_CHECK_DELAY;
  m_purgeInteval = purgeInterval;
  m_purgeCheckInterval = purgeCheckInterval;
  m_lastPurgeCheck = 0;
}

scJobQueueTask::~scJobQueueTask()
{
}

scString scJobQueueTask::getQueueName()
{
  return m_queueName;
}

scString scJobQueueTask::getTargetAddr()
{
  return m_targetAddr;
}

scString scJobQueueTask::getReturnAddr()
{
  return m_returnAddr;
}

bool scJobQueueTask::needsRun()
{
  return isTimeForTimeoutCheck() || isTimeForPurgeCheck();
}

void scJobQueueTask::intInit()
{
  openQueue();
}

int scJobQueueTask::intRun()
{
  int res = scTask::intRun();
  checkTimeoutsNeeded();
  checkPurgeCheckNeeded();
  res = res + 1;
  return res;
}

void scJobQueueTask::openQueue()
{
  emptyTarget();
  clearJobQueue();
  returnAllSubmittedInDb();
  loadJobsFromDb();
  checkActivateOnAllJobs();  
}

void scJobQueueTask::closeQueue()
{
  if (!m_closing) {
    m_closing = true;
    emptyTarget();
    clearJobQueue();
    returnAllSubmittedInDb();
    changeRunningToSleepingInDb();
  }
}

int scJobQueueTask::runStopping()
{
  if (!m_closing)
    closeQueue();
  return scTask::runStopping();  
}

int scJobQueueTask::handleMessage(scEnvelope &envelope, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  if (!envelope.getEvent()->isResponse()) {
    scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
    
    if (message->getInterface() == "job") 
    {
      res = handleQueueMsg(message, response);
    }    
  } else {
    res = SC_RESP_STATUS_OK;
  }
  return res;
}

int scJobQueueTask::handleQueueMsg(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "job") 
  {
    if (message->getCoreCommand() == "commit") {
      res = handleCmdCommit(message, response);
    } else if (message->getCoreCommand() == "alloc_res") {
      res = handleCmdAllocRes(message, response);
    } else if (message->getCoreCommand() == "set_vars") {
      res = handleCmdSetVars(message, response);
    } else if (message->getCoreCommand() == "log_text") {
      res = handleCmdLogText(message, response);
    } else if (message->getCoreCommand() == "start") {
      res = handleCmdStartJob(message, response);
    } else if (message->getCoreCommand() == "ended") {
      res = handleCmdJobEnded(message, response);
    } else if (message->getCoreCommand() == "dealloc_res") {
      res = handleCmdDeallocRes(message, response);
    } else if (message->getCoreCommand() == "rollback") {
      res = handleCmdRollback(message, response);
    } else if (message->getCoreCommand() == "get_state") {
      res = handleCmdGetState(message, response);
    } else if (message->getCoreCommand() == "restart") {
      res = handleCmdRestartJob(message, response);
    } else if (message->getCoreCommand() == "return") {
      res = handleCmdReturnJob(message, response);
    } else if (message->getCoreCommand() == "purge") {
      res = handleCmdPurgeJob(message, response);
    } else if (message->getCoreCommand() == "disp_vars") {
      res = handleCmdDispVars(message, response);
    } else if (message->getCoreCommand() == "stop") {
      res = handleCmdStopJob(message, response);
    }  
  }        
  return res;
}


int scJobQueueTask::handleCmdStartJob(scMessage *message, scResponse &response)
{  
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("name"))
  {
    scString jobName = params.getString("name");
    if (startJob(jobName, params))
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

int scJobQueueTask::handleCmdGetState(scMessage *message, scResponse &response)
{  
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id"))
  {
    ulong64 jobId = params.getUInt64("job_id");
    if (getJobState(jobId, response))
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

int scJobQueueTask::handleCmdLogText(scMessage *message, scResponse &response)
{  
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id") && params.hasChild("text"))
  {
    ulong64 jobId = params.getUInt64("job_id");
    uint severity = params.getUInt("severity", jltInfo);
    uint msgCode = params.getUInt("code", JMC_OTHER);
    scString text = params.getString("text");

    if (severity < jltMax) {       
      addJobLogEntry(jobId, scJobLogType(severity), msgCode, text);
      res = SC_MSG_STATUS_OK;
    }  
  } 
  
  return res;
}

int scJobQueueTask::handleCmdSetVars(scMessage *message, scResponse &response)
{  
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id"))
  {
    ulong64 jobId = params.getUInt64("job_id");
    uint lockId = stringToUInt(params.getString("lock_id"));
    ulong64 transId = params.getUInt64("trans_id", 0);    
    if (setVars(jobId, lockId, transId, params["vars"])) {
      response.setStatus(SC_MSG_STATUS_OK);
      res = SC_MSG_STATUS_OK;
    }  
  } 
  
  return res;
}

bool scJobQueueTask::startJob(const scString &defName, const scDataNode &params)
{
  ulong64 jobDefId = getJobDefId(defName);
  scDataNode coreParams, jobParams, param;
  scString paramName;
  
  if (jobDefId == 0)
    return false;
  prepareJobStartParams(jobDefId, params, coreParams, jobParams);        
  scString command = coreParams.getString("command");    
  scString sStartPaused = coreParams.getString("start_paused", "0");    

  uint log_level = jmlError;
  if (coreParams.getString(scString(JMM_JDEF_CLASS_SYS)+"_log_level", "") == "all")
    log_level = jmlAll;
  else  
    coreParams.getInt(scString(JMM_JDEF_CLASS_SYS)+"_log_level", jmlError);    
    
  uint msg_level = jmlError;
  if (coreParams.getString(scString(JMM_JDEF_CLASS_SYS)+"_msg_level", "") == "all")
    msg_level = jmlAll;
  else  
    msg_level = coreParams.getInt(scString(JMM_JDEF_CLASS_SYS)+"_msg_level", jmlError);    
    
  uint priority = coreParams.getInt(scString(JMM_JDEF_CLASS_SYS)+"_priority", JMM_DEF_PRIORITY);    
  uint job_timeout = coreParams.getInt(scString(JMM_JDEF_CLASS_SYS)+"_job_timeout", 0);    
  uint trans_timeout = coreParams.getInt(scString(JMM_JDEF_CLASS_SYS)+"_trans_timeout", 0);    
  uint retry_limit = coreParams.getInt(scString(JMM_JDEF_CLASS_SYS)+"_retry_limit", 0);    
  bool trans_sup = coreParams.getBool(scString(JMM_JDEF_CLASS_SYS)+"_trans_sup", false);    
  int initStatus = ((sStartPaused == "true") || (sStartPaused == "1"))?jsPaused:jsReady; 

  scDataNode njobParams;  
  scString paramName1;
  
  for(int i=0,epos=jobParams.size(); i!=epos; i++)
  {
    jobParams.getElement(i, param);
    paramName1 = jobParams.getElementName(i);
    if (paramName1.substr(0, scString(JMM_JDEF_CLASS_JOB).length()+1) != scString(JMM_JDEF_CLASS_JOB)+"_") 
      continue;
    paramName = paramName1.substr(scString(JMM_JDEF_CLASS_JOB).length()+1);  
    njobParams.addChild(paramName, new scDataNode(param.getAsString()));
  }  
  
  ulong64 jobId;
  uint lockId;
  
  insertJobToDb(jobDefId, command, initStatus, log_level, msg_level, priority, job_timeout, trans_timeout, retry_limit, trans_sup,
    njobParams, jobId, lockId);
  scDateTime nowDt = getCurrentDateTimeFromDb();  
  insertJobToJobQueue(jobId, command, initStatus, lockId, log_level, msg_level, priority, job_timeout, 
    trans_timeout, retry_limit, trans_sup, njobParams, "", nowDt, nowDt);
  
  checkActivateJob(jobId);
  return true;
}

void scJobQueueTask::insertJobToDb(ulong64 jobDefId, const scString &command, uint initStatus, uint logLevel, 
  uint msgLevel, uint priority, uint jobTimeout, uint commitTimeout, uint retryLeft, bool transSup, 
  const scDataNode &jobParams, ulong64 &jobId, uint &lockId)
{
  scDataNode values;
  
  values.addChild("job_def_id", new scDataNode(jobDefId));
  values.addChild("queue", new scDataNode(getQueueName()));
  values.addChild("command", new scDataNode(command));
  values.addChild("status", new scDataNode(initStatus));
  values.addChild("log_level", new scDataNode(logLevel));
  values.addChild("msg_level", new scDataNode(msgLevel));
  values.addChild("priority", new scDataNode(priority));
  values.addChild("job_timeout", new scDataNode(jobTimeout));
  values.addChild("trans_timeout", new scDataNode(commitTimeout));
  values.addChild("retry_left", new scDataNode(retryLeft));
  values.addChild("trans_sup", new scDataNode(transSup));
  lockId = 0;
  values.addChild("lock_id", new scDataNode(lockId));
  values.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));  
  
  m_db->startTrans();
  try {
    m_db->insertData("job", &values);
    
    jobId = m_db->getLastInsertedId();

    values.clear();
    values.addChild("job_id", new scDataNode(jobId));
    values.addChild("name", new scDataNode(""));
    values.addChild("value", new scDataNode(""));
    
    for(int i=0,epos=jobParams.size(); i!=epos; i++)
    {
      values.setString("name", jobParams.getElementName(i));
      values.setString("value", jobParams.getString(i));
      m_db->insertData("job_param", &values);
    }    
    m_db->commit();
  } catch(...) {
    m_db->rollback();
    throw;
  }
}

void scJobQueueTask::loadJobsFromDb()
{
  const int jsPendingStatuses[] = {
    jsPaused,
    jsWaiting,
    jsReady,
    jsSubmitted,
    jsRunning,
    jsSleep
  }; 
  const int jsPendingStatusesCnt = 6;

  scDataNode whereParams;
  scDataNode statusList, jobParams, row;
  scString paramName;
  
  for(int i=0; i < jsPendingStatusesCnt; i++)     
  {
    paramName = "st"+toString(i);
    whereParams.addChild(paramName, new scDataNode(jsPendingStatuses[i]));
    statusList.addChild(new scDataNode("{"+paramName+"}"));
  }
  whereParams.addChild("queue", new scDataNode(getQueueName()));
     
  scDbBase::cursor_transporter cur = 
    m_db->select(
      scString(
        "select * "
        "from job "
        "where queue = {queue} and status in ("
      )+
      statusList.implode(",")+
      scString(
        ")"
      ), 
      &whereParams
    );

  int cnt = 0;
  while (!cur->eof())
  {
    row = cur->fetch();
    
    getJobParams(row.getUInt64("job_id"), jobParams);
    
    insertJobToJobQueue(row, jobParams);      
    cnt++;
  }  
    
#ifdef JQT_LOG_ENABLED  
  scLog::addText("("+getQueueName()+") "+toString(cnt)+" jobs loaded on queue start"); 
#endif  
}

bool scJobQueueTask::loadOneJobFromDb(ulong64 jobId)
{
  bool res = false;
  scDataNode whereParams;
  scDataNode jobParams, row;
  
  whereParams.addChild("job_id", new scDataNode(jobId));
  if (
    m_db->getRow(
      scString(
        "select * "
        "from job "
        "where job_id = {job_id}"),
      &whereParams,
      row
    )
  )
  {
    getJobParams(row.getUInt64("job_id"), jobParams);    
    insertJobToJobQueue(row, jobParams);      
    res = true;
  }
  return res;
}

void scJobQueueTask::checkActivateOnAllJobs()
{
  scDataNode element;
  for(int i=0,epos=m_jobList.size(); i != epos; i++)
  {
    m_jobList.getElement(i, element);
    checkActivateJob(element.getUInt64("job_id"), true);
  }
}

void scJobQueueTask::getJobParams(ulong64 jobId, scDataNode &jobParams)
{
  scDataNode whereParams;
  scDataNode vectorParams, element;
  
  whereParams.addChild("job_id", new scDataNode(jobId));
  jobParams.clear();  
  m_db->getRowsAsVector("select name, value from job_param where job_id = {job_id}", &whereParams, vectorParams);  

  for(int i=0,epos = vectorParams.size(); i != epos; i++)
  {
    vectorParams.getElement(i, element);
    jobParams.addChild(element.getString("name"), new scDataNode(element.getString("value")));
  }      
}

void scJobQueueTask::clearJobQueue()
{
  m_jobList.clear();
}

void scJobQueueTask::insertJobToJobQueue(const scDataNode &jobData, const scDataNode &jobParams) {
  scDateTime jobStartDt = isoStrToDateTime(jobData.getString("started_dt"));
  scDateTime transStartDt = isoStrToDateTime(getLatestTransStartDt(jobData.getUInt64("job_id")));
  
  insertJobToJobQueue(
    jobData.getUInt64("job_id"),
    jobData.getString("command"),
    jobData.getUInt("status"),
    jobData.getUInt("lock_id"),
    jobData.getUInt("log_level"),
    jobData.getUInt("msg_level"),
    jobData.getUInt("priority"),
    jobData.getUInt("job_timeout"),
    jobData.getUInt("trans_timeout"),
    jobData.getUInt("retry_left"),
    jobData.getBool("trans_sup"),
    jobParams,
    jobData.getString("worker_addr"),
    jobStartDt,
    transStartDt
  );  
}

void scJobQueueTask::insertJobToJobQueue(ulong64 jobId, const scString &command, uint initStatus, uint lockId, 
  uint logLevel, uint msgLevel, uint priority, uint jobTimeout, uint commitTimeout, uint retryLeft, bool transSup, 
  const scDataNode &jobParams, const scString &workerAddr, scDateTime jobStartDt, scDateTime transStartDt)
{
  std::auto_ptr<scDataNode> newEntry(new scDataNode(toString(jobId)));
  
  newEntry->addChild("job_id", new scDataNode(jobId));
  newEntry->addChild("command", new scDataNode(command));
  newEntry->addChild("status", new scDataNode(initStatus));
  newEntry->addChild("log_level", new scDataNode(logLevel));
  newEntry->addChild("msg_level", new scDataNode(msgLevel));
  newEntry->addChild("priority", new scDataNode(priority));
  newEntry->addChild("job_timeout", new scDataNode(jobTimeout));
  newEntry->addChild("trans_timeout", new scDataNode(commitTimeout));
  newEntry->addChild("retry_left", new scDataNode(retryLeft));
  newEntry->addChild("lock_id", new scDataNode(lockId));
  newEntry->addChild("worker_addr", new scDataNode(workerAddr));
  newEntry->addChild("trans_sup", new scDataNode(transSup));
  newEntry->addChild("started_dt", new scDataNode(dateTimeToIsoStr(jobStartDt)));
  newEntry->addChild("trans_start_dt", new scDataNode(dateTimeToIsoStr(transStartDt)));
  
  newEntry->addChild("job_params", new scDataNode(jobParams));
  m_jobList.addChild(newEntry.release());
}

/// merge: base params, definition params & start params
void scJobQueueTask::prepareJobStartParams(ulong64 jobDefId, const scDataNode &inParams, scDataNode &outCoreParams, scDataNode &outJobParams)
{
  static scDataNode coreParamNames = scDataNode::explode(",", "name,base,command", true);
  static scDataNode coreStartParamNames = scDataNode::explode(",", "name,queue,command", true);

  scDataNode jobDefRow, jobDefParams;
  scDataNode jobBaseRow, jobBaseParams;
  scDataNode param;
  ulong64 jobBaseId = 0;
  scString entryName;
  scDataNode *paramDest;
  bool sysParam;
  
  getJobDefInfo(jobDefId, jobDefRow, jobDefParams);
  
  if (jobDefRow.getString("base").length() > 0)
  {
    jobBaseId = getJobDefId(jobDefRow.getString("base"));
    getJobDefInfo(jobBaseId, jobBaseRow, jobBaseParams);    
  } 
  
  if (jobBaseId != 0)
  {
    outCoreParams = jobBaseRow;
  } 
  
  if (!outCoreParams.hasChild("command"))
    outCoreParams.addChild("command", new scDataNode(""));    

  if (jobDefRow.getString("command").length() > 0)  
    outCoreParams.setString("command", jobDefRow.getString("command"));

  // add parameters from base
  for(int i=0,epos=jobBaseParams.size(); i!=epos; i++)
  {
    jobBaseParams.getElement(i, param);
    if (coreParamNames.hasChild(param.getString("name"))) 
      continue;

    sysParam = (param.getString("param_class") == scString(JMM_JDEF_CLASS_SYS));
    if (sysParam) 
      paramDest = &outCoreParams;
    else
      paramDest = &outJobParams;  
      
    entryName = param.getString("param_class")+"_"+param.getString("name");
    if (!paramDest->hasChild(entryName))
      paramDest->addChild(entryName, new scDataNode(param.getString("value")));
    else         
      (*paramDest)[entryName].setAsString(param.getString("value"));
  }

  // add parameters from definition
  for(int i=0,epos=jobDefParams.size(); i!=epos; i++)
  {
    jobDefParams.getElement(i, param);
    if (coreParamNames.hasChild(param.getString("name"))) 
      continue;
    sysParam = (param.getString("param_class") == scString(JMM_JDEF_CLASS_SYS));
    if (sysParam) 
      paramDest = &outCoreParams;
    else
      paramDest = &outJobParams;  
      
    entryName = param.getString("param_class")+"_"+param.getString("name");
    if (!paramDest->hasChild(entryName))
      paramDest->addChild(entryName, new scDataNode(param.getString("value")));
    else         
      (*paramDest)[entryName].setAsString(param.getString("value"));
  }

  // add start input parameters
  for(int i=0,epos=inParams.size(); i!=epos; i++)
  {
    inParams.getElement(i, param);
    entryName = inParams.getElementName(i);

    if (coreStartParamNames.hasChild(entryName)) 
      continue;


    if (entryName.substr(0,1) != "_") {
      entryName = scString(JMM_JDEF_CLASS_JOB)+"_"+entryName;
      paramDest = &outJobParams;  
    } else {  
      entryName = scString(JMM_JDEF_CLASS_SYS)+"_"+(entryName.substr(1));
      paramDest = &outCoreParams;
    }  

    if (!paramDest->hasChild(entryName))
      paramDest->addChild(entryName, new scDataNode(param));
    else         
      (*paramDest)[entryName].copyValueFrom(param);
  }
}

void scJobQueueTask::getJobDefInfo(ulong64 jobDefId, scDataNode &coreParams, scDataNode &jobParams)
{
  scDataNode whereParams;
  whereParams.addChild("job_def_id", new scDataNode(jobDefId));
  if (!m_db->getRow("select base,command from job_def where job_def_id = {job_def_id}", &whereParams, coreParams)) 
    throw scError("Job definition not found: "+toString(jobDefId));
  
  jobParams.clear();  
  m_db->getRowsAsVector("select name, param_class, value from job_def_param where job_def_id = {job_def_id}", &whereParams, jobParams);
}

ulong64 scJobQueueTask::getJobDefId(const scString &name)
{
  scDataNode defValue(0);
  scDataNode params;
  scDataNode idNode;
  
  params.addChild("name", new scDataNode(name));
  m_db->getValue("select job_def_id from job_def where name = {name}", &params, idNode, &defValue);
  ulong64 res = stringToUInt64(idNode.getAsString());
  return res;
}

// check if job can be activated = moved to target queue
void scJobQueueTask::checkActivateJob(ulong64 jobId, bool awake)
{
  scString keyName = toString(jobId);
  if (m_jobList.hasChild(keyName))
  {  
    uint status = m_jobList[keyName].getUInt("status");
    if ((status == jsReady) || ((status == jsSleep) && awake))
      activateJob(jobId);
  }  
}

// build "job_worker.start_work" message and add it to target squeue
void scJobQueueTask::activateJob(ulong64 jobId)
{
  static scDataNode copyParamNames = scDataNode::explode(",", "job_id,lock_id,trans_id,command,log_level,msg_level,priority,job_params", true);
  
  scString keyName = toString(jobId);
  scDataNode &jobEntry = m_jobList[keyName];
  ulong64 transId = 0;
  
  if (jobEntry.getBool("trans_sup"))
    if (!startTrans(jobId, transId))
      transId = 0;
    
  scDataNode params, element;
  
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(m_targetAddr));
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job_worker.start_work");
  messageGuard->setRequestId(getScheduler()->getNextRequestId());

  for(int i=0,epos=jobEntry.size(); i != epos; i++)
  { 
    jobEntry.getElement(i, element);
    if (copyParamNames.hasChild(jobEntry.getElementName(i)))
      params.addChild(jobEntry.getElementName(i), new scDataNode(element));
  }
  
  params.addChild("return_addr", new scDataNode(m_returnAddr));  
  if (transId != 0)
    params.addChild("trans_id", new scDataNode(transId));  
    
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());
  changeJobStatus(jobId, jsSubmitted);
  getScheduler()->postEnvelope(envelopeGuard.release(), 
    new scJobSubmitHandler(this, jobId, jobEntry.getUInt("lock_id")));
}

void scJobQueueTask::handleSubmitSuccess(ulong64 jobId, const scString &workerAddr, uint lockId)
{
  if (changeJobWorker(jobId, workerAddr, lockId)) {
    changeJobStatus(jobId, jsRunning, lockId);
    addJobLogEntry(jobId, jltInfo, JMC_JOB_START, scString("Job started @ ")+workerAddr);
#ifdef JQT_LOG_ENABLED       
      scLog::addInfo("Job ["+toString(jobId)+"] started @ "+workerAddr);
#endif          
  } else {
    addJobLogEntry(jobId, jltWarning, JMC_SUB_UNKNOWN, 
      toString("False submit success received from: ")+
      workerAddr+scString(" with lock: ")+
      toString(lockId)
    );  
  }
}

bool scJobQueueTask::changeJobWorker(ulong64 jobId, const scString &workerAddr, uint lockId)
{
  bool res;
  scDataNode whereParams;
  scDataNode values;
  scDataNode expr;
  
  whereParams.addChild("job_id", new scDataNode(jobId));
  whereParams.addChild("lock_id", new scDataNode(lockId));
  
  values.addChild("worker_addr", new scDataNode(workerAddr));
  values.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

  res = (m_db->updateData("job", &values, SC_NULL, &whereParams) > 0);
  if (res) {
    scString keyName = toString(jobId);
    scDataNode &jobEntry = m_jobList[keyName];
    jobEntry.setString("worker_addr", workerAddr);
  }
  
  return res;
}

void scJobQueueTask::handleSubmitError(ulong64 jobId, const scString &errorMsg, uint lockId)
{
  changeJobStatus(jobId, jsAborted, lockId);
  addJobLogEntry(jobId, jltError, JMC_SUB_ERROR, scString("Submit error: "+errorMsg+", lock: "+toString(lockId)));
}

void scJobQueueTask::addJobOperStatus(ulong64 jobId, const scString &operName, bool success, uint msgCodeOK, uint msgCodeErr)
{
  scString msg = operName+": ";
  if (success) 
    addJobLogEntry(jobId, jltInfo, msgCodeOK, msg+"OK");
  else
    addJobLogEntry(jobId, jltError, msgCodeErr, msg+"failed");
}

void scJobQueueTask::addJobLogEntry(ulong64 jobId, scJobLogType a_type, int messageCode, const scString &message)
{
  scDataNode values;
  scDataNode expr;
  values.addChild("job_id", new scDataNode(jobId));
  values.addChild("msg_type", new scDataNode(static_cast<uint>(a_type)));
  values.addChild("msg_code", new scDataNode(messageCode));
  values.addChild("message", new scDataNode(message));  
  //values.addChild(new scDataNode("added_dt", getTimestamp()));  

  values.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));
  
  m_db->insertData("job_log", &values);
}

void scJobQueueTask::changeJobStatus(ulong64 jobId, scJobStatus newStatus, long64 lockId)
{
  scDataNode whereParams;
  scDataNode values;
  scDataNode expr;

  whereParams.addChild("job_id", new scDataNode(jobId));
  if (lockId >= 0)  
    whereParams.addChild("lock_id", new scDataNode((uint)lockId));
  if (newStatus == jsRunning)  
    values.addChild(scDbBase::newExpr("started_dt", "datetime('now')"));
    
  values.addChild("status", new scDataNode(static_cast<uint>(newStatus)));
  values.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

  m_db->updateData("job", &values, SC_NULL, &whereParams);
  
  scString keyName = toString(jobId);
  scDataNode &jobEntry = m_jobList[keyName];
  jobEntry.setUInt("status", newStatus);
  if (values.hasChild("started_dt"))
    jobEntry.setString("started_dt", dateTimeToIsoStr(getCurrentDateTimeFromDb()));
  if (jobEntry.hasChild("updated_dt"))
    jobEntry.setString("updated_dt", dateTimeToIsoStr(getCurrentDateTimeFromDb()));
  
  // TODO: send notification "job_client.status_changed job_id=<job_id>,status=<status>", req params: client_addr
}

scString scJobQueueTask::getTimestamp()
{
  scDataNode defValue("", 0);
  scDataNode resNode;
  
  m_db->getValue("select datetime('now')", SC_NULL, resNode, &defValue);
  return resNode.getAsString();
}

scDateTime scJobQueueTask::getCurrentDateTimeFromDb()
{
  scString str = getTimestamp();
  scDateTime dtInDb = isoStrToDateTime(str);
  return dtInDb;
}

// params: job_id, lock_id
int scJobQueueTask::handleCmdJobEnded(scMessage *message, scResponse &response)
{  
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id") && params.hasChild("lock_id"))
  { 
    ulong64 jobId = stringToUInt64(params.getString("job_id"));
    uint lockId = stringToUInt(params.getString("lock_id"));
    ulong64 transId = params.getUInt64("trans_id", 0);
    bool resultOK = params.getBool("success", true);
    scString errorMsg;
    
    if (!resultOK) 
      errorMsg = params.getString("error_msg", "");
      
    if (handleJobEnded(jobId, lockId, transId, resultOK, errorMsg))
      res = SC_MSG_STATUS_OK;
  }
  
  return res;
}

bool scJobQueueTask::handleJobEnded(ulong64 jobId, uint lockId, ulong64 transId, bool resultOK, const scString &errorMsg)
{
  bool res;
  
  res = jobExists(jobId, lockId);

  if (res)
  {
    res = isCurrentJobLock(jobId, lockId);
    if (!res)
      addJobLogEntry(jobId, jltWarning, JMC_JOB_END, scString("Worker ended job with lock not current"));
  } 
  
  if (res) {
    try {
      if (resultOK) {
        if (transId > 0) {
            commitJob(jobId, lockId, transId);
        }          
        changeJobStatus(jobId, jsEnded, lockId);      
        addJobLogEntry(jobId, jltInfo, JMC_JOB_END, scString("Job ended"));
#ifdef JQT_LOG_ENABLED       
        scLog::addInfo("Job ["+toString(jobId)+"] ended successfully after: "+toString(dateTimeToMSecs(getJobExecTime(jobId))/1000.0)+"s");
#endif      
     } else {
        if (transId > 0) {
          rollbackJob(jobId, lockId, transId);
        }  
        changeJobStatus(jobId, jsAborted);      
        addJobLogEntry(jobId, jltInfo, JMC_JOB_END, scString("Job ended with error: ["+errorMsg+"]"));
#ifdef JQT_LOG_ENABLED       
        scLog::addInfo("Job ["+toString(jobId)+"] ended with error: ["+errorMsg+"]");
#endif      
     }
     
    } 
    catch(const std::exception& e) {
      changeJobStatus(jobId, jsAborted, lockId);      
      addJobLogEntry(jobId, jltError, JMC_JOB_END_ERROR, scString("Job end error: [")+e.what()+"], lock: "+toString(lockId));
    }
  }
  
  return res;
}

bool scJobQueueTask::jobExists(ulong64 jobId, long64 lockId)
{
  bool res;
  
  scString keyName = toString(jobId);
  res = m_jobList.hasChild(keyName);
  if (res && (lockId >= 0)) {
    scDataNode &jobEntry = m_jobList[keyName];
    res = (jobEntry.getUInt("lock_id") == uint(lockId));
  }        

  return res;
}

// Clear target queue. 
// note: unsupported now, because:
// - there can be only one writer when using this function (no "revoke" message implemented)
// - no way to obtain queue name required for squeue.clear
// - not really needed now (squeue on the same node as job queue)
void scJobQueueTask::emptyTarget()
{
  //std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope());
  //scMessageAddress targetAddr(m_targetAddr);
  //targetAddr.setTask("");  
  //envelopeGuard->setReceiver(targetAddr);
  //
  //std::auto_ptr<scMessage> messageGuard(new scMessage());
  //messageGuard->setCommand("squeue.clear");
  //envelopeGuard->setEvent(messageGuard.release());
  //getScheduler()->postEnvelope(envelopeGuard.release());
}

// - change status from submitted to ready
// - increment lock_id for each modified job
// Note: in-memory job queue needs to be empty (statuses will be changed after this function)
void scJobQueueTask::returnAllSubmittedInDb()
{
  scDataNode setValues, whereParams;

  setValues.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));
  setValues.addChild(scDbBase::newExpr("lock_id", "lock_id + 1"));  
  setValues.addChild("status", new scDataNode(static_cast<uint>(jsReady)));    
  
  whereParams.addChild("status", new scDataNode(static_cast<uint>(jsSubmitted)));    
  whereParams.addChild("queue", new scDataNode(getQueueName()));    
  
  m_db->updateData("job", &setValues, SC_NULL, &whereParams);
}

void scJobQueueTask::changeRunningToSleepingInDb()
{
  scDataNode setValues, whereParams;

  setValues.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));
  setValues.addChild(scDbBase::newExpr("lock_id", "lock_id + 1"));  
  setValues.addChild("status", new scDataNode(static_cast<uint>(jsSleep)));    
  
  whereParams.addChild("status", new scDataNode(static_cast<uint>(jsRunning)));    
  whereParams.addChild("queue", new scDataNode(getQueueName()));    
  
  m_db->updateData("job", &setValues, SC_NULL, &whereParams);
}

bool scJobQueueTask::startTrans(ulong64 jobId, ulong64 &transId)
{
  scDataNode values;
  bool res;
  
  values.addChild("job_id", new scDataNode(jobId));
  values.addChild("trans_closed", new scDataNode(false));
  values.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));
    
  res = (m_db->insertData("job_trans", &values) > 0);
    
  transId = m_db->getLastInsertedId();
  res = res && (transId > 0);
  if (res)
  {
    setTransStartDt(jobId, getTimestamp());
  }
  return res;
}

bool scJobQueueTask::clearTransStartDt(ulong64 jobId)
{
  return setTransStartDt(jobId, toString(0));
}

bool scJobQueueTask::setTransStartDt(ulong64 jobId, const scString &value)
{
  bool res;
  scString keyName = toString(jobId);
  if (isJobInQueue(jobId))
  {
    if (!m_jobList[keyName].hasChild("trans_start_dt"))
      m_jobList[keyName].addChild("trans_start_dt", new scDataNode());        
    m_jobList[keyName].setString("trans_start_dt", value);   
    res = true;
  }  
  else
    res = false;
  return res;  
}

bool scJobQueueTask::commitJob(ulong64 jobId, uint lockId, ulong64 transId)
{
  bool res;
  assert(transId != 0);
  assert(jobId != 0);
  res = isCurrentJobLock(jobId, lockId);
  if (res) {
    m_db->startTrans();
    try {  
      clearTransStartDt(jobId);
      if (res)
        res = commitJobCopyVarsToBase(jobId, transId);
      if (res)
        res = removeJobVars(jobId, transId);
      if (res)  
        res = removeAllocations(jobId, transId, true);
      if (res)
        res = closeTrans(jobId, transId);  

      addJobOperStatus(jobId, "Commit", res, JMC_JOB_COMMIT_OK, JMC_JOB_COMMIT_ERR);
      m_db->commit();
    } catch(...) {
      m_db->rollback();
      throw;
    }  
  }
  return res;
}

bool scJobQueueTask::rollbackJob(ulong64 jobId, uint lockId, ulong64 transId, bool ignoreLock)
{
  bool res;
  assert(transId != 0);
  assert(jobId != 0);
  res = ignoreLock || isCurrentJobLock(jobId, lockId);

  if (res)
    clearTransStartDt(jobId);  
  if (res)
    res = removeJobVars(jobId, transId);
  if (res) 
    res = removeAllocations(jobId, transId, false);
  if (res)
    res = closeTrans(jobId, transId);  

  addJobOperStatus(jobId, "Rollback", res, JMC_JOB_ROLLBACK_OK, JMC_JOB_ROLLBACK_ERR);
  return false;
}

uint scJobQueueTask::getCurrentJobLock(ulong64 jobId)
{
  uint res;
  scDataNode whereParams,value;
  whereParams.addChild("job_id", new scDataNode(jobId));
  if (m_db->getValue("select lock_id from job where job_id = {job_id}", &whereParams, value))
    res = value.getAsUInt();
  else
    res = 0;  
  return res;  
}

bool scJobQueueTask::isCurrentJobLock(ulong64 jobId, uint lockId)
{
  bool res = (getCurrentJobLock(jobId) == lockId);
  return res;  
}

// returns all state variables in response result as
// vector of named values.
bool scJobQueueTask::getJobState(ulong64 jobId, scResponse &response)
{
  scDataNode namedVars;
  bool res = getJobStateData(jobId, namedVars);
  response.setStatus(SC_MSG_STATUS_OK);
  response.setResult(namedVars);  
  return res;
}

bool scJobQueueTask::getJobStateData(ulong64 jobId, scDataNode &output)
{
  bool res = true;
  scDataNode whereParams;
  
  whereParams.addChild("job_id", new scDataNode(jobId));
  whereParams.addChild("trans_id", new scDataNode(0)); // read only commited values
  
  scDataNode rows, element;
  output.clear();
  
  m_db->getRowsAsVector( 
        "select var_name, var_value "
        "from job_state "
        "where job_id = {job_id} and trans_id = {trans_id}",
        &whereParams, 
        rows);
  
  for(int i=0,epos=rows.size(); i != epos; i++)
  {
    rows.getElement(i, element);
    output.addChild(element.getString("var_name"), new scDataNode(element.getString("var_value")));
  }
  
  return res;
}

bool scJobQueueTask::setVars(ulong64 jobId, uint lockId, ulong64 transId, const scDataNode &vars)
{
  scDataNode jobStateAk, whereParamsVar, insertValues, setValues;
  bool useTrans = (vars.size() > 1);

  if ((transId == 0) && !isCurrentJobLock(jobId, lockId))
    return false;  
  
  jobStateAk.addChild("job_id", new scDataNode(jobId));
  jobStateAk.addChild("trans_id", new scDataNode(transId));

  if (useTrans) m_db->startTrans();
  try {
    for(int i=0,epos=vars.size(); i != epos; i++)
    {
      whereParamsVar = jobStateAk;
      whereParamsVar.addChild("var_name", new scDataNode(vars.getElementName(i)));
      
      setValues.clear();
      setValues.addChild("var_value", new scDataNode(vars.getString(i)));
      setValues.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));
          
      if (m_db->updateData("job_state", &setValues, SC_NULL, &whereParamsVar) <= 0)
      {
        insertValues = jobStateAk;
        insertValues.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));
        insertValues.addChild("var_name", new scDataNode(vars.getElementName(i)));
        insertValues.addChild("var_value", new scDataNode(vars.getString(i)));
        m_db->insertData("job_state", &insertValues);
      }    
    }  
   if (useTrans) m_db->commit();
  } catch(...) {
    if (useTrans) m_db->rollback();
    throw;
  }  
  return true;
}

// input: job_id, trans_id, [one or more: name=<logic_name>,type="file",path=<path>]
int scJobQueueTask::handleCmdAllocRes(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  bool operRes = true;

  scDataNode &params = message->getParams(); 
  if ((params.hasChild("list")) && (params.isArray() || params.isParent()))
  { // array of resources
    scDataNode element, list;
    list = params["list"];
    bool useTrans = (list.size()>1);
    if (useTrans) m_db->startTrans();
    try {        
      for(int i=0,epos=list.size(); i != epos; i++)
      { 
        list.getElement(i, element);
        if (!allocJobRes(params, element))
          operRes = false;
      }  
      if (useTrans) m_db->commit();
    } catch(...) {
      if (useTrans) m_db->rollback();
      throw;
    }        
  } else {
    if (!allocJobRes(params, params))
      operRes = false;
  }
  
  if (operRes)  
    res = SC_MSG_STATUS_OK;
  
  return res;
}

// input: job_id, trans_id, [one or more: name=<logic_name>]
int scJobQueueTask::handleCmdDeallocRes(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  bool operRes = true;

  scDataNode &params = message->getParams(); 
  if ((params.hasChild("list")) && (params.isArray() || params.isParent()))
  { // array of resources
    scDataNode element, list;
    list = params["list"];
    m_db->startTrans();
    try {    
      for(int i=0,epos=list.size(); i != epos; i++)
      { 
        list.getElement(i, element);
        if (!deallocJobRes(params, element))
          operRes = false;
      }  
      m_db->commit();
    } catch(...) {
      m_db->rollback();
      throw;
    }    
  } else {
    if (!deallocJobRes(params, params))
      operRes = false;
  }
  
  if (operRes)  
    res = SC_MSG_STATUS_OK;

  return res;
}

bool scJobQueueTask::allocJobRes(const scDataNode &head, const scDataNode &item)
{
  bool res = false; 
  
  if (head.hasChild("job_id") && item.hasChild("name") && item.hasChild("path") && item.hasChild("type")
    && (head.getUInt64("trans_id", 0)>0 || head.getUInt("lock_id", 0)>0))
  {
    ulong64 jobId = head.getUInt64("job_id");
    uint lockId = head.getUInt("lock_id", 0);
    ulong64 transId = head.getUInt64("trans_id", 0);
    scString name = item.getString("name");
    scString path = item.getString("path");    
    scString resType = item.getString("type");
    
    if (allocJobRes(jobId, lockId, transId, name, path, resType))
      res = true;
  } 
  return res;  
}

bool scJobQueueTask::allocJobRes(ulong64 jobId, uint lockId, ulong64 transId, const scString &name, const scString &path, 
  const scString &resType)
{
  if ((transId == 0) && !isCurrentJobLock(jobId, lockId))
    return false;  

  scDataNode values;
  values.addChild("job_id", new scDataNode(jobId));
  values.addChild("trans_id", new scDataNode(transId));
  values.addChild("name", new scDataNode(name));
  values.addChild("res_path", new scDataNode(path));
  values.addChild("res_type", new scDataNode(resType));
  values.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));

  bool res = (m_db->insertData("job_res", &values) > 0);
  return res;
}

bool scJobQueueTask::deallocJobRes(const scDataNode &head, const scDataNode &item)
{
  bool res = false;
  if (head.hasChild("job_id") && item.hasChild("name")
    && (head.getUInt64("trans_id", 0)>0 || head.getUInt("lock_id", 0)>0))
  {
    ulong64 jobId = head.getUInt64("job_id");
    uint lockId = head.getUInt("lock_id", 0);
    ulong64 transId = head.getUInt64("trans_id", 0);
    scString name = item.getString("name");
    
    if (deallocJobRes(jobId, lockId, transId, name))
      res = true;     
  }   
  return res;
}
  
bool scJobQueueTask::deallocJobRes(ulong64 jobId, uint lockId, ulong64 transId, const scString &name)
{
  if ((transId == 0) && !isCurrentJobLock(jobId, lockId))
    return false;  

  scDataNode values;
  values.addChild("job_id", new scDataNode(jobId));
  values.addChild("trans_id", new scDataNode(transId));
  values.addChild("name", new scDataNode(name));

  bool res = (m_db->deleteData("job_res", &values) > 0);
  return res;
}

int scJobQueueTask::handleCmdCommit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id") && params.hasChild("trans_id"))
  {
    ulong64 jobId = params.getUInt64("job_id");
    ulong64 transId = params.getUInt64("trans_id");
    uint lockId = params.getUInt("lock_id");
    bool chained = params.getBool("chained", true);
    bool resOK = commitJob(jobId, lockId, transId);
    
    if (resOK && chained) {
      scDataNode result;
      ulong64 newTransId;
      resOK = startTrans(jobId, newTransId);
      if (resOK) {
        result.addChild("trans_id", new scDataNode(newTransId));
        response.setResult(result);
      }
    }
    
    if (resOK)
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

int scJobQueueTask::handleCmdRollback(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id") && params.hasChild("trans_id"))
  {
    ulong64 jobId = params.getUInt64("job_id");
    ulong64 transId = params.getUInt64("trans_id");
    uint lockId = params.getUInt("lock_id");
    bool chained = params.getBool("chained", true);
    bool resOK = rollbackJob(jobId, lockId, transId);
    
    if (resOK && chained) {
      scDataNode result;
      ulong64 newTransId;
      resOK = startTrans(jobId, newTransId);
      if (resOK) {
        result.addChild("trans_id", new scDataNode(newTransId));
        response.setResult(result);
      }
    }
    
    if (resOK)
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

bool scJobQueueTask::commitJobCopyVarsToBase(ulong64 jobId, ulong64 transId)
{
  scString sqla = 
    "update job_state "
    "set var_value = (select var_value from job_state ts1 "
    "  where job_state.var_name = ts1.var_name and "
    "  job_state.job_id = ts1.job_id and "
    "  ts1.trans_id = {trans_id}"
    ") "
    "where job_id = {job_id} and trans_id = {base_trans_id} and exists ("
    "select 1 from job_state ts2 where "
    "ts2.job_id = {job_id} and "
    "ts2.trans_id = {trans_id} and "
    "ts2.var_name = job_state.var_name "
    ")";

  scDataNode whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  whereParams.addChild("trans_id", new scDataNode(transId));
  whereParams.addChild("base_trans_id", new scDataNode(0));
  
  m_db->execute(sqla, &whereParams);
  
  scString sqlb = 
    "insert into job_state(job_id, trans_id, var_name, var_value, added_dt, updated_dt) "
    "select ts1.job_id, {base_trans_id}, ts1.var_name, ts1.var_value, ts1.added_dt, ts1.updated_dt "
    "from job_state ts1 "
    "where "
    "  ts1.job_id = {job_id} and "
    "  ts1.trans_id = {trans_id} and "
    "  not exists(select 1 from job_state ts2 where "
    "    ts2.job_id = {job_id} and "
    "    ts2.trans_id = {base_trans_id} and "
    "    ts2.var_name = ts1.var_name "
    "  )";

  m_db->execute(sqlb, &whereParams);
  return true;
}

bool scJobQueueTask::removeJobVars(ulong64 jobId, ulong64 transId)
{
  scDataNode whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  whereParams.addChild("trans_id", new scDataNode(transId));
  
  m_db->deleteData("job_state", &whereParams);
  return true;
}

bool scJobQueueTask::rollbackAllJobTrans(ulong64 jobId)
{
  scDataNode whereParams, transList, element;
  whereParams.addChild("job_id", new scDataNode(jobId));

  scString sql = 
  "select trans_id from job_trans where job_id = {job_id} and trans_closed = 'F'";
  
  m_db->getRowsAsVector(sql, &whereParams, transList);
  for(int i=0,epos=transList.size(); i != epos; i++)
  {
    transList.getElement(i, element);
    rollbackJob(jobId, 0, element.getUInt64("trans_id"), true);
  }  
  
  return true;
}

bool scJobQueueTask::removeAllocations(ulong64 jobId)
{
  return removeAllocations(jobId, 0, false, true);
}

bool scJobQueueTask::removeAllocations(ulong64 jobId, ulong64 transId, bool asCommit, bool allTypes)
{
  bool res;

  res = removeResAllocations(jobId, transId, JMM_RESTYP_TEMPFILE);

  if (allTypes || (!asCommit))
    res = removeResAllocations(jobId, transId, JMM_RESTYP_WORKFILE);
  else
    res = removeAllocationsFromDb(jobId, transId, JMM_RESTYP_WORKFILE);
    
  if (allTypes || asCommit)
    res = removeResAllocations(jobId, transId, JMM_RESTYP_OBSOLFILE);
  else
    res = removeAllocationsFromDb(jobId, transId, JMM_RESTYP_OBSOLFILE);

  if (allTypes)  
    res = removeResAllocations(jobId, transId, "*");    
  
  return res;  
}

bool scJobQueueTask::removeResAllocations(ulong64 jobId, ulong64 transId, const scString &resType)
{
  scDataNode whereParams, rows, element;
  whereParams.addChild("job_id", new scDataNode(jobId));
  if (transId > 0)
    whereParams.addChild("trans_id", new scDataNode(transId));
  whereParams.addChild("res_type", new scDataNode(resType));

  scString sql = 
  "select res_path from job_res where job_id = {job_id} and trans_id = {trans_id} and res_type = {res_type}";
  
  m_db->getRowsAsVector(sql, &whereParams, rows);
  for(int i=0,epos=rows.size(); i != epos; i++)
  {
    rows.getElement(i, element);
    removeTempFileByPath(element.getString("res_path"));
  }  
  
  removeAllocationsFromDb(jobId, transId, resType);
  return true;  
}

bool scJobQueueTask::removeAllocationsFromDb(ulong64 jobId, ulong64 transId, const scString &resType)
{
  scDataNode whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  if (transId > 0)
    whereParams.addChild("trans_id", new scDataNode(transId));
  
  if (resType != "*")
    whereParams.addChild("res_type", new scDataNode(resType));
  
  m_db->deleteData("job_res", &whereParams);
  return true;
}

bool scJobQueueTask::removeTempFileByPath(const scString &a_path)
{
  bool res = false;
  std::string stdpath = a_path.c_str();
  if (canRemoveFile(a_path))
    res = (remove(stdpath.c_str()) != -1);
  return res;  
}

bool scJobQueueTask::canRemoveFile(const scString &a_path)
{
  scString upPath = extractDir(strToUpper(a_path));
  bool res = (m_safeRootList.size() > 0);
  scDataNode element;
  
  for(int i=0,epos=m_safeRootList.size(); i != epos; i++)
  {
    m_safeRootList.getElement(i, element);
    // if safe path is a start of a given file path
    if (upPath.find(element.getAsString()) == 0)
    {
      res = true;
      break;
    }    
  }
  
  return res;
}

bool scJobQueueTask::closeTrans(ulong64 jobId, ulong64 transId)
{
  scDataNode whereParams, setValues;
  whereParams.addChild("job_id", new scDataNode(jobId));
  whereParams.addChild("trans_id", new scDataNode(transId));
  
  setValues.addChild("trans_closed", new scDataNode(true));
  setValues.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

  bool res = (m_db->updateData("job_trans", &setValues, SC_NULL, &whereParams) > 0);
  return res;
}

ulong64 scJobQueueTask::getJobIdParam(const scDataNode &params) 
{
  ulong64 jobId;
  if (params.hasChild("job_id"))
    jobId = params.getUInt64("job_id");
  else
    jobId = params.getUInt64(0);
  return jobId;  
}

int scJobQueueTask::handleCmdRestartJob(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (!params.empty())
  {
    ulong64 jobId = getJobIdParam(params);
    if (restartJob(jobId))
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

int scJobQueueTask::handleCmdReturnJob(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (!params.empty())
  {
    ulong64 jobId = getJobIdParam(params);

    if (returnJob(jobId))
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

int scJobQueueTask::handleCmdStopJob(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (!params.empty())
  {
    ulong64 jobId = getJobIdParam(params);

    if (stopJob(jobId))
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

int scJobQueueTask::handleCmdPurgeJob(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (!params.empty())
  {
    ulong64 jobId = getJobIdParam(params);
        
    if (purgeJob(jobId))
      res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}

// Restart job - start from the beginning. Current status must be <> purged.
// - increase lock
// - if status "submit" or "running" send message to worker "job_worker.cancel_work <job_id>"
// - rollback all pending transactions
// - remove all state values
// - change status to "ready"
// - check activate job
bool scJobQueueTask::restartJob(ulong64 jobId)
{
  uint status = getJobStatus(jobId);
  bool res;
  res = prepareJobInQueue(jobId);
  if (res) 
    res = (status != jsNull);
  if (res)
    res = increaseJobLock(jobId);
  if (res)
    cancelJobOnWorker(jobId);
  if (res)
    res = rollbackAllJobTrans(jobId);  
  if (res)
    res = removeJobVars(jobId);  
  if (res)  
    changeJobStatus(jobId, jsReady);    
  if (res)  
    checkActivateJob(jobId);

#ifdef JQT_LOG_ENABLED       
   if (res) scLog::addInfo("Job ["+toString(jobId)+"] restarted");
#endif      

  addJobOperStatus(jobId, "Restart", res, JMC_JOB_RESTART_OK, JMC_JOB_RESTART_ERR);
  return res;  
}

bool scJobQueueTask::abortJob(ulong64 jobId)
{
  uint status = getJobStatus(jobId);
  bool res;
  res = prepareJobInQueue(jobId);
  if (res) 
    res = ((status != jsPurged) && (status != jsNull));
  if (res)
    res = increaseJobLock(jobId);
  if (res)
    cancelJobOnWorker(jobId);
  if (res)
    res = rollbackAllJobTrans(jobId);  
  if (res)  
    changeJobStatus(jobId, jsAborted);    

#ifdef JQT_LOG_ENABLED       
   if (res) scLog::addInfo("Job ["+toString(jobId)+"] aborted");
#endif      

  addJobOperStatus(jobId, "Abort", res, JMC_JOB_ABORT_OK, JMC_JOB_ABORT_ERR);
  return res;  
}

// Return job to queue - only if still in submit or running state.
// - increase lock
// - if status "submit" or "running" send message to worker "job_worker.cancel_work <job_id>"
// - rollback all pending transactions
// - change status to "ready"
// - check activate job
bool scJobQueueTask::returnJob(ulong64 jobId)
{
  bool res = false;
  uint status = getJobStatus(jobId);
  if (
      (status == jsSubmitted) || 
      (status == jsReady) || 
      (status == jsRunning) || 
      (status == jsSleep) || 
      (status == jsPaused) || 
      (status == jsAborted) 
     ) 
  {    
    res = prepareJobInQueue(jobId); 
    if (res)
      res = increaseJobLock(jobId);
    if (res)
      cancelJobOnWorker(jobId);
    if (res)
      res = rollbackAllJobTrans(jobId);  
    if (res)  
      changeJobStatus(jobId, jsReady);    
    if (res)  
      checkActivateJob(jobId);
  }  

#ifdef JQT_LOG_ENABLED       
   if (res) scLog::addInfo("Job ["+toString(jobId)+"] returned");
#endif      
  addJobOperStatus(jobId, "Return", res, JMC_JOB_RETURN_OK, JMC_JOB_RETURN_ERR);
  return res;  
}

bool scJobQueueTask::stopJob(ulong64 jobId)
{
  bool res = false;
  uint status = getJobStatus(jobId);
  if ((status == jsSubmitted) || 
      (status == jsReady) || 
      (status == jsRunning) || 
      (status == jsSleep) || 
      (status == jsPaused)) 
  {
    res = prepareJobInQueue(jobId); 
    if (res)
      res = increaseJobLock(jobId);
    if (res)
      cancelJobOnWorker(jobId);
    if (res)
      res = rollbackAllJobTrans(jobId);  
    if (res)  
      changeJobStatus(jobId, jsAborted);    
  }  

#ifdef JQT_LOG_ENABLED       
   if (res) scLog::addInfo("Job ["+toString(jobId)+"] stopped");
#endif      
  addJobOperStatus(jobId, "Stop", res, JMC_JOB_STOP_OK, JMC_JOB_STOP_ERR);
  return res;  
}

// clean after job is stopped:
// - remove allocations
// - remove vars
// - remove transactions
// - clear log
// - change status to "purged"
bool scJobQueueTask::purgeJob(ulong64 jobId)
{
  bool res = false;
  uint status = getJobStatus(jobId);
  if ((status == jsAborted) || (status == jsEnded) || (status == jsPurged)) {
    res = prepareJobInQueue(jobId); 
    if (res)
      res = rollbackAllJobTrans(jobId);  
    if (res)
      res = removeAllocations(jobId);
    if (res)
      res = removeJobVars(jobId);        
    if (res)
      res = removeJobTrans(jobId);        
    if (res)
      res = clearJobLog(jobId);        
    if (res)  
      changeJobStatus(jobId, jsPurged);    
  }  

#ifdef JQT_LOG_ENABLED       
   if (res) scLog::addInfo("Job ["+toString(jobId)+"] purged");
#endif      
  return res;  
}

uint scJobQueueTask::getJobStatus(ulong64 jobId)
{
  uint res = jsNull;
  bool found = false;
  scString keyName = toString(jobId);

  if (isJobInQueue(jobId)) 
    found = true;
  else if (prepareJobInQueue(jobId))
    found = true;

  if (found)    
    res = m_jobList[keyName].getUInt("status");
  return res;    
}

//warning: calculates how long ago job started from now
scDateTime scJobQueueTask::getJobExecTime(ulong64 jobId)
{
  scDateTime res = 0;
  scDataNode whereParams, row;
  whereParams.addChild("job_id", new scDataNode(jobId));
  if (m_db->getRow("select started_dt, updated_dt from job where job_id = {job_id}", &whereParams, row))
  {
    scDateTime startedDt = isoStrToDateTime(row.getString("started_dt", "0")); 
    scDateTime updatedDt = isoStrToDateTime(row.getString("updated_dt", "0")); 
    res = dateTimeDiff(updatedDt, startedDt);
  }
  return res;
}

bool scJobQueueTask::isJobInQueue(ulong64 jobId)
{
  bool res;
  scString keyName = toString(jobId);
  res = m_jobList.hasChild(keyName);
  return res;
}

bool scJobQueueTask::prepareJobInQueue(ulong64 jobId)
{
  bool res = true;
  scString keyName = toString(jobId);
  if (!m_jobList.hasChild(keyName)) 
    res = loadOneJobFromDb(jobId);
  return res;  
}      

bool scJobQueueTask::increaseJobLock(ulong64 jobId) 
{
  scDataNode values, whereParams;
  values.addChild(scDbBase::newExpr("lock_id", "lock_id + 1"));
  values.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));
  whereParams.addChild("job_id", new scDataNode(jobId));
  bool res = (m_db->updateData("job", &values, SC_NULL, &whereParams) > 0);
  if (res) {
    scString keyName = toString(jobId);
    if (!m_jobList.hasChild(keyName)) { 
      res = false; 
    } else {
      scDataNode &jobEntry = m_jobList[keyName];
      jobEntry.setUInt("lock_id", jobEntry.getUInt("lock_id")+1);
    }
  }
  return res;
}

bool scJobQueueTask::removeJobVars(ulong64 jobId)
{
  scDataNode values, whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  m_db->deleteData("job_state", &whereParams);
  return true;
}

bool scJobQueueTask::removeJobTrans(ulong64 jobId)
{
  scDataNode values, whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  m_db->deleteData("job_trans", &whereParams);
  return true;
}

bool scJobQueueTask::clearJobLog(ulong64 jobId)
{
  scDataNode values, whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  m_db->deleteData("job_log", &whereParams);
  return true;
}

void scJobQueueTask::cancelJobOnWorker(ulong64 jobId)
{
  scDataNode whereParams, row;
  whereParams.addChild("job_id", new scDataNode(jobId));
  if (m_db->getRow("select worker_addr, status from job where job_id = {job_id}", &whereParams, row))
  {
    scString addr;
    if (!row["worker_addr"].isNull())
      addr = row.getString("worker_addr");
    uint status = row.getUInt("status");  

    if ((addr.length() > 0) && ((status == jsSubmitted) || (status == jsRunning)))
    {
      postCancelJobOn(jobId, addr);
    }
  }
}

void scJobQueueTask::postCancelJobOn(ulong64 jobId, const scString &addr)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(addr));
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("job_worker.cancel_work");
  scDataNode params;
  params.addChild("job_id", new scDataNode(jobId));    
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());
  getScheduler()->postEnvelope(envelopeGuard.release());
}

scString scJobQueueTask::getLatestTransStartDt(ulong64 jobId)
{
  scDataNode defValue(0);
  scDataNode resNode;
  scDataNode whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  m_db->getValue("select max(added_dt) from job_trans where job_id = {job_id}", &whereParams, resNode, &defValue);
  return resNode.getAsString();
}

bool scJobQueueTask::isTimeForTimeoutCheck()
{
  bool res;
  cpu_ticks nowTicks = dateTimeToMSecs(currentDateTime());
  if (cpu_time_delay(m_lastTimeoutCheck, nowTicks) > m_timeoutDelay)
    res = true;
  else
    res = false;
  return res;    
}

bool scJobQueueTask::isTimeForPurgeCheck()
{
  bool res = false;
  if (m_lastPurgeCheck == 0) {
    res = true; 
  } else {
    cpu_ticks nowTicks = dateTimeToMSecs(currentDateTime());
    if (cpu_time_delay(m_lastPurgeCheck, nowTicks) > m_purgeCheckInterval)
      res = true;
  }  
  return res;
}

void scJobQueueTask::checkPurgeCheckNeeded()
{
  if (isTimeForPurgeCheck())
  {
    m_lastPurgeCheck = dateTimeToMSecs(currentDateTime());
    runGlobalPurgeCheck();
  }
}

void scJobQueueTask::checkTimeoutsNeeded()
{
  cpu_ticks nowTicks = dateTimeToMSecs(currentDateTime());
  if (cpu_time_delay(m_lastTimeoutCheck, nowTicks) > m_timeoutDelay)
  {
    m_lastTimeoutCheck = nowTicks;
    runTimeoutCheck();
  }
}

void scJobQueueTask::runTimeoutCheck()
{
  scDataNode element;
  scDateTime nowDt = getCurrentDateTimeFromDb();
  for(int i=0,epos=m_jobList.size(); i != epos; i++)
  {
    m_jobList.getElement(i, element);
    checkTimeoutsForJob(element.getUInt64("job_id"), 
      element.getBool("trans_sup", false),   
      element.getUInt("job_timeout"),
      element.getUInt("trans_timeout"),
      isoStrToDateTime(element.getString("started_dt", "0")), 
      isoStrToDateTime(element.getString("trans_start_dt", "0")),
      nowDt);
  }
}

void scJobQueueTask::checkTimeoutsForJob(ulong64 jobId, bool transSup, uint jobTimeout, uint transTimeout, 
  scDateTime jobStartDt, scDateTime transStartDt, scDateTime nowDt)
{
  scJobStatus status = scJobStatus(getJobStatus(jobId));
  if ((status == jsRunning) || (status == jsSubmitted) || (status == jsWaiting))
  {
    if ((jobTimeout > 0) && !isNullDateTime(jobStartDt))
    {
      scDateTime diff = dateTimeDiff(jobStartDt, nowDt);
      if (dateTimeToMSecs(diff) > jobTimeout)
        handleJobTimeout(jobId);
    } 
    if (transSup && (transTimeout > 0) && (transStartDt > 0))
    {
      scDateTime diff = dateTimeDiff(transStartDt, nowDt);
      if (dateTimeToMSecs(diff) > transTimeout)
        handleTransTimeout(jobId);
    }
  }
}

void scJobQueueTask::handleJobTimeout(ulong64 jobId)
{
  addJobLogEntry(jobId, jltError, JMC_JOB_TIMEOUT, scString("Job timeout"));
  uint retryCnt = m_jobList[toString(jobId)].getUInt("retry_left", 0);
  if (retryCnt > 0) {
    setRetryCount(jobId, retryCnt - 1);
    restartJob(jobId);
  } else {
    abortJob(jobId);
  }  
}

void scJobQueueTask::handleTransTimeout(ulong64 jobId)
{
  addJobLogEntry(jobId, jltError, JMC_TRANS_TIMEOUT, scString("Trans. timeout"));

  uint retryCnt = m_jobList[toString(jobId)].getUInt("retry_left", 0);
  if (retryCnt > 0) {
    setRetryCount(jobId, retryCnt - 1);
    returnJob(jobId);
  } else {
    abortJob(jobId);
  }  
}

void scJobQueueTask::setRetryCount(ulong64 jobId, uint value)
{
  if (isJobInQueue(jobId))
    m_jobList[toString(jobId)].setUInt("retry_left", value);
  
  scDataNode whereParams;
  scDataNode values;
  scDataNode expr;

  whereParams.addChild("job_id", new scDataNode(jobId));
  values.addChild("retry_left", new scDataNode((uint)value));
  values.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));
  m_db->updateData("job", &values, SC_NULL, &whereParams);  
}

void scJobQueueTask::runGlobalPurgeCheck()
{
  scDateTime nowDt = getCurrentDateTimeFromDb();
  scDateTime purgeBeforeDt = nowDt - msecsToDateTime(m_purgeInteval);

  scDataNode whereParams;
  scDataNode jobList, element, statusValues;
    
  statusValues.addChild("st_purged", new scDataNode(static_cast<uint>(jsPurged)));
  statusValues.addChild("st_aborted", new scDataNode(static_cast<uint>(jsAborted)));
  statusValues.addChild("st_ended", new scDataNode(static_cast<uint>(jsEnded)));

  scString statusList = statusValues.childNames().implode("},{");
  if (statusList.length() > 0)
    statusList = "{"+statusList+"}";
    
  whereParams = statusValues;
  whereParams.addChild("before_dt", new scDataNode(dateTimeToIsoStr(purgeBeforeDt)));
  
  m_db->getRowsAsVector("select job_id from job where status in ("+statusList+") and updated_dt < {before_dt}", 
    &whereParams, jobList);  
  
  for(int i=0,epos = jobList.size(); i != epos; i++)    
  {
    jobList.getElement(i, element);
    purgeJobFull(element.getUInt64("job_id"));
  }      
}

// - remove all output
// - remove all params
// - remove job entry
void scJobQueueTask::purgeJobFull(ulong64 jobId)
{
  purgeJob(jobId);
  removeJob(jobId);
}

void scJobQueueTask::removeJob(ulong64 jobId)
{
  scDataNode whereParams;
  whereParams.addChild("job_id", new scDataNode(jobId));
  m_db->deleteData("job_param", &whereParams);
  m_db->deleteData("job", &whereParams);

  scString keyName = toString(jobId);
  if (m_jobList.hasChild(keyName))
    m_jobList.getChildren().erase(keyName);
}

int scJobQueueTask::handleCmdDispVars(scMessage *message, scResponse &response)
{  
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  if (params.hasChild("job_id"))
  {
    ulong64 jobId = params.getUInt64("job_id");
    scDataNode vars;
    if (getJobStateData(jobId, vars)) {
      scDataNode element;

      scLog::addText("name|value"); 
      for(int i=0,epos=vars.size(); i != epos; i++)
      {
        vars.getElement(i, element);
        scLog::addText(vars.getElementName(i)+"|"+element.getAsString());         
      }
      scLog::addText("--> "+toString(vars.size())+" match(es) found");       
    } else {
      scLog::addText(JMM_NO_MATCHES_TEXT); 
    }
    res = SC_MSG_STATUS_OK;
  } 
  
  return res;
}
