/////////////////////////////////////////////////////////////////////////////
// Name:        Job.cpp
// Project:     scLib
// Purpose:     Persistent job support.
// Author:      Piotr Likus
// Modified by:
// Created:     27/12/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////
#include "sc/DbSqlite.h"
#include "sc/utils.h"
#include "sc/log.h"

#include "grd/JobManagerModule.h"

// ----------------------------------------------------------------------------
// scJobManagerModule
// ----------------------------------------------------------------------------
scJobManagerModule::scJobManagerModule(): scModule()
{
  m_db = new scDbSqlite();
}

scJobManagerModule::~scJobManagerModule()
{
  delete m_db;
}

void scJobManagerModule::init()
{
}

scStringList scJobManagerModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("job");
  return res;
}

scDbBase *scJobManagerModule::getDatabase()
{
  return m_db;
}

void scJobManagerModule::setDbPath(const scString &path)
{
  m_dbPath = path;
}

scString scJobManagerModule::getDbPath()
{
  return m_dbPath;
}

void scJobManagerModule::setSafeRootList(const scString &value)
{
  m_safeRootList = value;
}

scString scJobManagerModule::getSafeRootList()
{
  return m_safeRootList;
}

void scJobManagerModule::initDatabase()
{
  m_db->connect(m_dbPath);
  if (!(m_db->tableExists("job_def")))
    prepareDbStructure();
}

void scJobManagerModule::prepareDbStructure()
{
  scString sql =
  scString(
  "create table job_def(job_def_id integer primary key, name varchar(32), base varchar(32), command varchar(1024), trans_sup char(1), added_dt DATETIME, updated_dt DATETIME);"
  "CREATE UNIQUE INDEX job_def_ix1 ON job_def(name);"
  "create table job_def_param(job_def_param_id integer primary key, job_def_id integer, name varchar(32), param_class varchar(3), value varchar(4096));"
  "CREATE UNIQUE INDEX job_def_param_ix1 ON job_def_param(job_def_id, name);"
  "create table job(job_id integer primary key, job_def_id integer, status integer, lock_id integer, queue varchar(32), worker_addr varchar(64),"
  "  command varchar(1024), priority integer, log_level integer, msg_level integer, job_timeout integer, trans_timeout integer, trans_sup char(1), retry_left integer, added_dt DATETIME, updated_dt DATETIME, started_dt DATETIME);"
  "CREATE INDEX job_ix1 ON job(queue);"
  "create table job_param(job_param_id integer primary key, job_id integer, name varchar(32), value varchar(1024));"
  "CREATE UNIQUE INDEX job_param_ix1 ON job_param(job_id, name);"
  "create table job_log(job_log_id integer primary key, job_id integer, msg_type integer, msg_code integer, message varchar(1024), added_dt DATETIME);"
  "CREATE INDEX job_job_ix1 ON job_log(job_id);"
  "create table job_state(job_state_id integer primary key, job_id integer, trans_id integer, var_name varchar(32), var_value varchar(1024), added_dt DATETIME, updated_dt DATETIME);"
  "CREATE UNIQUE INDEX job_state_ix1 ON job_state(job_id, trans_id, var_name);"
  "create table job_res(job_res_id integer primary key, job_id integer, trans_id integer, name varchar(32), res_path varchar(1024), res_type varchar(32), added_dt DATETIME);"
  "CREATE UNIQUE INDEX job_res_ix1 ON job_res(job_id, trans_id, name);"
  "create table job_trans(trans_id integer primary key, job_id integer, trans_closed char(1), added_dt DATETIME, updated_dt DATETIME);"
  "CREATE INDEX job_trans_ix1 ON job_trans(job_id);"
  );
  scDataNode sqlList = scDataNode::explode(";", sql);
  scString cmd;

  for(int i=0,epos=sqlList.size();i!=epos;i++)
  {
    cmd = sqlList.getString(i);
    if (strTrim(cmd).length() > 0)
      m_db->execute(cmd);
  }
}

int scJobManagerModule::handleMessage(scMessage *message, scResponse &response)
{
  const char* byIdCmds = "|commit|rollback|alloc_res|dealloc_res|set_vars|ended|get_state|purge|disp_vars|stop|log_text|restart|";

  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "job")
  {
    scString wrk(byIdCmds);
    if (wrk.find(scString("|")+message->getCoreCommand()+"|") != scString::npos)
      res = forwardQueueMsgByJobId(message, response);
    else if (message->getCoreCommand() == "start") {
      res = handleCmdStartJob(message, response);
    } else if (message->getCoreCommand() == "restart") {
      res = forwardQueueMsg(message, response);
    } else if (message->getCoreCommand() == "return") {
      res = forwardQueueMsg(message, response);
    } else if (message->getCoreCommand() == "define") {
      res = handleCmdDefine(message, response);
    } else if (message->getCoreCommand() == "change_def") {
      res = handleCmdChangeDef(message, response);
    } else if (message->getCoreCommand() == "remove_def") {
      res = handleCmdRemoveDef(message, response);
    } else if (message->getCoreCommand() == "desc_def") {
      res = handleCmdDescDef(message, response);
    } else if (message->getCoreCommand() == "list_jobs") {
      res = handleCmdListJobs(message, response);
    } else if (message->getCoreCommand() == "list_defs") {
      res = handleCmdListDefs(message, response);
    } else if (message->getCoreCommand() == "list_queues") {
      res = handleCmdListQueues(message, response);
    } else if (message->getCoreCommand() == "init_manager") {
      res = handleCmdInitManager(message, response);
    } else if (message->getCoreCommand() == "start_queue") {
      res = handleCmdStartQueue(message, response);
    } else if (message->getCoreCommand() == "stop_queue") {
      res = handleCmdStopQueue(message, response);
    }
  }
  return res;
}

scTaskIntf *scJobManagerModule::prepareTaskForMessage(scMessage *message)
{
  scTask *res = SC_NULL;
  scString coreCmd = message->getCoreCommand();

  if (
     (message->getInterface() == "job")
     )
  {
  //init, listen, close, clear, get_status
    if (coreCmd == "start_queue")
    {
      res = prepareManager(message);
    }
  }

  return res;
}

int scJobManagerModule::handleCmdInitManager(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (!params.empty())
  {
    scString dbPath = params.getString(0);
    res = SC_MSG_STATUS_OK;
    setDbPath(dbPath);
    initDatabase();
  }
  return res;
}

//-------------------------------------------------------------------------------------------------
/// Define job.
/// Parameters:
///   name - job definition name
///   base - base definition name
///   command - one or more commands separated by ':'
///   system parameters - started with _sys_ - forwarded to job definition params - 'sys' type
///   job parameters - all other named parameters - forwarded to job parameters list - 'usr' type
/// Details - system parameters:
///   priority (int, lower = higher)
///   log_level (int)
///   log_address (default = '@log')
///   msg_level (int)
///   msg_address (default = '@console')
///   job_timeout
///   trans_timeout
///   wait_timeout
///   queue_name (default = <empty>)
///   trans_sup - transaction is supported
///   tokens_in (comma-separated, ex. "CND1,CND2,CND3=10")
///   tokens_out (comma-separated, ex. "CND1,CND2,CND3=10")
/// Returns:
///   status = OK|ERROR
//-------------------------------------------------------------------------------------------------
int scJobManagerModule::handleCmdDefine(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (!params.empty())
  {
    if (params.hasChild("name"))
      if (defineJob(params))
        res = SC_MSG_STATUS_OK;
  }
  return res;
}

bool scJobManagerModule::defineJob(const scDataNode &params)
{
  bool res;

  scString name;
  scString baseName, command, paramName;
  scDataNode sysParams, jobParams, param, nparams;

  std::auto_ptr<scDataNode> paramGuard;

  name = params.getString("name");
  baseName = params.getString("base", "");
  command = params.getString("command", "");

  processDefParams(params, nparams);

  for(int i=0, epos = nparams.size(); i != epos; i++)
  {
    nparams.getElement(i, param);
    paramName = nparams.getElementName(i);
    if (paramName.substr(0, 1) == "_")
    {
      paramGuard.reset(new scDataNode(param));
      sysParams.addChild(paramName.substr(1), paramGuard.release());
    } else {
      jobParams.addChild(paramName, new scDataNode(param));
    }
  }

  res = intDefineJob(name, baseName, command, sysParams, jobParams);
  return res;
}

bool scJobManagerModule::intDefineJob(const scString &name, const scString &baseName, const scString &command,
  const scDataNode &sysParams, const scDataNode &jobParams)
{
   scDataNode params;
   scDataNode newParam;

  m_db->startTrans();
  try {
   params.addChild("name", new scDataNode(name));
   params.addChild("base", new scDataNode(baseName));
   params.addChild("command", new scDataNode(command));
   //scDataNode expr;
   //expr.addChild(new scDataNode("expression", true));
   //expr.addChild(new scDataNode("value", "datetime('now')"));
   //params.addChild(new scDataNode("added_dt", expr));
   params.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));

   m_db->insertData("job_def", &params);
   ulong64 jobDefId = m_db->getLastInsertedId();
   //-------
   for(int i=0,epos=sysParams.size();i != epos; i++)
   {
     params.clear();
     sysParams.getElement(i, newParam);
     params.addChild("job_def_id", new scDataNode(jobDefId));
     params.addChild("param_class", new scDataNode("sys"));
     params.addChild("name", new scDataNode(sysParams.getElementName(i)));
     params.addChild("value", new scDataNode(newParam));
     m_db->insertData("job_def_param", &params);
   }
   //-------
   for(int i=0,epos=jobParams.size();i != epos; i++)
   {
     params.clear();
     jobParams.getElement(i, newParam);
     params.addChild("job_def_id", new scDataNode(jobDefId));
     params.addChild("param_class", new scDataNode("job"));
     params.addChild("name", new scDataNode(jobParams.getElementName(i)));
     params.addChild("value", new scDataNode(newParam));
     m_db->insertData("job_def_param", &params);
   }
    m_db->commit();
  }
  catch (...) {
    m_db->rollback();
    throw;
  }
  return true;
}

/*
bool scJobManagerModule::prepareJobParams(const scString &name, const scDataNode &params)
{
  loaded = false;
  if (baseName.length() > 0)
    loaded = loadJobDef(baseName, baseParams);
  if (!loaded)
    loaded = loadJobDef(JMM_DEFAULT_DEF_NAME, baseParams);
  if (!loaded)
    loaded = fillDefaultsForJobDef(baseParams);
}
*/


int scJobManagerModule::handleCmdChangeDef(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (!params.empty())
  {
    if (params.hasChild("name"))
      if (changeJobDef(params))
        res = SC_MSG_STATUS_OK;
  }
  return res;
}

/// remove core params from list
void scJobManagerModule::processDefParams(const scDataNode &input, scDataNode &output)
{
  static scDataNode coreParamNames = scDataNode::explode(",", "name,base,command", true);
  scDataNode param;
  scString paramName;

  for(int i=0, epos = input.size(); i != epos; i++)
  {
    input.getElement(i, param);
    paramName = input.getElementName(i);
    if (!coreParamNames.hasChild(paramName)) {
      output.addChild(new scDataNode(param));
    }
  }
}

bool scJobManagerModule::changeJobDef(scDataNode &changeParams)
{
  ulong64 jobDefId = getJobDefId(changeParams.getString("name"));
  scDataNode whereParams, setParams, jobParams, newParam;
  scString paramName;

  whereParams.addChild("job_def_id", new scDataNode(jobDefId));

  if (changeParams.hasChild("base"))
    setParams.addChild("base", new scDataNode(changeParams.getString("base")));
  if (changeParams.hasChild("command"))
    setParams.addChild("command", new scDataNode(changeParams.getString("command")));

   //scDataNode expr;
   //expr.addChild(new scDataNode("expression", true));
   //expr.addChild(new scDataNode("value", "datetime('now')"));
   //setParams.addChild(new scDataNode("updated_dt", expr));
  setParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

  m_db->startTrans();
  try {
    if (setParams.size())
      m_db->updateData("job_def", &setParams, SC_NULL, &whereParams);

    processDefParams(changeParams, jobParams);

   for(int i=0,epos=jobParams.size();i != epos; i++)
   {
     whereParams.clear();
     setParams.clear();

     jobParams.getElement(i, newParam);
     paramName = jobParams.getElementName(i);
     if (paramName.substr(0,1) == "_")
       paramName = paramName.substr(1);
     whereParams.addChild("job_def_id", new scDataNode(jobDefId));
     whereParams.addChild("name", new scDataNode(paramName));

     setParams.addChild("value", new scDataNode(newParam));

     m_db->updateData("job_def_param", &setParams, SC_NULL, &whereParams);
   }

   m_db->commit();
  }
  catch (...) {
    m_db->rollback();
    throw;
  }

  return true;
}

ulong64 scJobManagerModule::getJobDefId(const scString &name)
{
  scDataNode params;
  scDataNode idNode;
  scDataNode defValue("", 0);

  params.addChild("name", new scDataNode(name));
  m_db->getValue("select job_def_id from job_def where name = {name}", &params, idNode, &defValue);
  ulong64 res = stringToUInt64(idNode.getAsString());
  return res;
}

int scJobManagerModule::handleCmdRemoveDef(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (!params.empty())
  {
    scString name = params.getString(0);
    if (removeJobDef(name)) {
      res = SC_MSG_STATUS_OK;
    }
  }
  return res;
}

bool scJobManagerModule::removeJobDef(const scString &name)
{
  scDataNode params;
  ulong64 jobDefId = getJobDefId(name);
  params.clear();
  params.addChild("job_def_id", new scDataNode(jobDefId));
  ulong64 rows;
  m_db->startTrans();
  try {
    rows = m_db->deleteData("job_def_param", &params);
    rows += m_db->deleteData("job_def", &params);
    m_db->commit();
  }
  catch (...) {
    m_db->rollback();
    throw;
  }
  return (rows > 0);
}

int scJobManagerModule::handleCmdListDefs(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();
  scString filter;

  if (params.empty())
    filter = "*";
  else
    filter = params.getString(0);

  listJobDefs(filter);
  res = SC_MSG_STATUS_OK;
  return res;
}

int scJobManagerModule::handleCmdListJobs(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();
  scString filter;

  if (params.empty())
    filter = "*";
  else
    filter = params.getString(0);

  listJobs(filter);
  res = SC_MSG_STATUS_OK;
  return res;
}

void scJobManagerModule::listJobDefs(const scString &filter)
{
  scDataNode listing;
  scString sqlPattern = prepareSqlPatternFor(filter);
//  scString sqlPattern = "%";
  scDataNode whereParams, row;
  scString sLine;

  whereParams.addChild("pattern", new scDataNode(sqlPattern));
  scDbBase::cursor_transporter cur = m_db->select("select name, base, command from job_def where name like {pattern}", &whereParams);

  scLog::addText("name|base|command");
  if (cur->eof())
    scLog::addText(JMM_NO_MATCHES_TEXT);
  else {
    int cnt = 0;
    while (!cur->eof())
    {
      row = cur->fetch();
//      if (wildcard_match(row[0].getAsString(), filter)) {
      sLine = "";
      sLine += row[0].getAsString()+"|";
      sLine += row[1].getAsString()+"|";
      sLine += row[2].getAsString();
      scLog::addText(sLine);
      cnt++;
//      }
    }
    scLog::addText("--> "+toString(cnt)+" match(es) found");
  }
}

void scJobManagerModule::listJobs(const scString &filter)
{
  scDataNode listing;
  scString sqlPattern = prepareSqlPatternFor(filter);
//  scString sqlPattern = "%";
  scDataNode whereParams, row;
  scString sLine;

  whereParams.addChild("pattern", new scDataNode(sqlPattern));
  scDbBase::cursor_transporter cur =
    m_db->select("select j.job_id, jd.name, j.status, j.queue, j.priority, j.command, j.added_dt, j.updated_dt from job j, job_def jd where j.job_def_id = jd.job_def_id and j.queue like {pattern} order by j.status, jd.name ", &whereParams);

  scLog::addText("job_id|name|status|queue|priority|command|added_dt|updated_dt");
  int cnt = 0;
  uint statusId;
  scDataNode statuses = scDataNode::explode(",",JMM_STATUS_NAMES);
  scString statusName;

  while (!cur->eof())
  {
    row = cur->fetch();
    sLine = "";
    sLine += row[0].getAsString()+"|";
    sLine += row[1].getAsString()+"|";
    statusId = stringToUInt(row[2].getAsString());
    if (statusId < statuses.size())
      statusName = statuses.getElement(statusId).getAsString();
    else
      statusName = "?";
    sLine += statusName+"|";
    sLine += row[3].getAsString()+"|";
    sLine += row[4].getAsString()+"|";
    sLine += row[5].getAsString()+"|";
    //just to test scDateTime formatting/parsing:
    //sLine += datetime_to_iso_str(iso_str_to_datetime(row[6].getAsString()))+"|";
    sLine += row[6].getAsString()+"|";
    sLine += row[7].getAsString();
    scLog::addText(sLine);
    cnt++;
  }
  scLog::addText("--> "+toString(cnt)+" match(es) found");
}

scString scJobManagerModule::prepareSqlPatternFor(const scString &filter)
{
  scString res;
  for(int i=0, cnt=filter.length(); i < cnt; i++)
  {
    if (filter[i] == '*')
      res += "%";
    else if (filter[i] == '?')
      res += "_";
    else
      res += filter[i];
  }

  return res;
}

int scJobManagerModule::handleCmdDescDef(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();

  if (!params.empty())
  {
    scString name = params.getString(0);
    if (descJobDef(name)) {
      res = SC_MSG_STATUS_OK;
    }
  }
  return res;
}

bool scJobManagerModule::descJobDef(const scString &name)
{
  scDataNode listing;
//  scString sqlPattern = prepareSqlPatternFor(filter);
  scDataNode whereParams, row;
  scString sLine;
  ulong64 jobDefId = getJobDefId(name);

  whereParams.addChild("job_def_id", new scDataNode(jobDefId));
  scDbBase::cursor_transporter cur = m_db->select("select name, base, command from job_def where job_def_id = {job_def_id}", &whereParams);

  if (cur->eof())
    scLog::addText("--> 0 matches found");
  else {
    scLog::addText("-- Job def header --");
    scLog::addText("name|base|command");

    while (!cur->eof())
    {
      row = cur->fetch();
      sLine = "";
      sLine += row[0].getAsString()+"|";
      sLine += row[1].getAsString()+"|";
      sLine += row[2].getAsString();
      scLog::addText(sLine);
    }

    scDbBase::cursor_transporter cur2 = m_db->select("select name, param_class, value from job_def_param where job_def_id = {job_def_id} order by param_class, name", &whereParams);

    scLog::addText("-- Job def parameters --");
    scLog::addText("name|class|value");
    int cnt = 0;
    while (!cur2->eof())
    {
      row = cur2->fetch();
      sLine = "";
      sLine += row[0].getAsString()+"|";
      sLine += row[1].getAsString()+"|";
      sLine += row[2].getAsString();
      scLog::addText(sLine);
      cnt++;
    }

    scLog::addText("--> "+toString(cnt)+" parameter(s) found");
  }

  return (jobDefId != 0);
}

/// if queue is not already created:
/// - create job queue task
/// - add to scheduler
int scJobManagerModule::handleCmdStartQueue(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams();
  response.initFor(*message);

  if (!params.empty()) {
    scString qname;

    qname = params.getString(0);
    if (!qname.empty()) {
      if (queueExists(qname))
      {
   	    res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Queue already running: ["+qname+"]");
        response.setError(error);
      } else { // no queue yet
   	    res = SC_MSG_STATUS_TASK_REQ;
      }
    } // qname filled
  } // has children

  return res;
}

bool scJobManagerModule::queueExists(const scString &name)
{
  return (findQueue(name) != SC_NULL);
}

scJobQueueTask *scJobManagerModule::findQueue(const scString &name)
{
  scJobQueueTask *res = SC_NULL;
  scString pname;

  scJobQueueTaskList::const_iterator p;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    pname = (*p)->getQueueName();
    if (pname == name)
    {
      res = *p;
      break;
    }
  }
  return res;
}

scJobQueueTask *scJobManagerModule::checkQueue(const scString &name)
{
  scJobQueueTask *res = findQueue(name);
  if (res == SC_NULL)
    throw scError("Unknown queue: ["+name+"]");
  return res;
}

void scJobManagerModule::closeQueue(const scString &name)
{
  scJobQueueTask *manager = checkQueue(name);
  m_managers.remove(manager);
  manager->closeQueue();
  scScheduler *scheduler = manager->getScheduler();
  scheduler->deleteTask(manager);
}

void scJobManagerModule::setUnknownQueueError(const scString &qname, scResponse &response)
{
  scDataNode error;
  error.setElementSafe("text", "Unknown queue: ["+qname+"]");
  response.setError(error);
}

scTask *scJobManagerModule::prepareManager(scMessage *message)
{
  scTask *res = SC_NULL;

  scDataNode &params = message->getParams();

  if (!params.empty()) {
    scString qname = params.getString("name", "");
    scString targetAddr = params.getString("target", "");
    scString returnAddr = params.getString("return", "");
    ulong64 purgeInterval = params.getUInt64("purge_interval", JQ_DEF_PURGE_INTERVAL);
    if (purgeInterval == 0)
      purgeInterval = JQ_DEF_PURGE_INTERVAL;
    ulong64 purgeCheckInterval = params.getUInt64("purge_check_interval", JQ_DEF_PURGE_CHECK_INTERVAL);
    if (purgeCheckInterval == 0)
      purgeCheckInterval = JQ_DEF_PURGE_CHECK_INTERVAL;
    if (!qname.empty() && !targetAddr.empty()) {
      res = startQueue(qname, targetAddr, returnAddr, purgeInterval, purgeCheckInterval);
    }
  }

  return res;
}

scJobQueueTask *scJobManagerModule::startQueue(const scString &name, const scString &targetAddr,
  const scString &returnAddr, ulong64 purgeInterval, ulong64 purgeCheckInterval)
{
  std::auto_ptr<scJobQueueTask> guard;

  if (queueExists(name))
    throw scError("Queue already exists: ["+name+"]");
  guard.reset(new scJobQueueTask(name, targetAddr, returnAddr, getDatabase(), getSafeRootList(), purgeInterval, purgeCheckInterval));
  scJobQueueTask *res = guard.get();
  res->setName(scString(JMM_JQMAN_PFX)+name);
  m_managers.push_back(res);

  return guard.release();
}

int scJobManagerModule::handleCmdStopQueue(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams();
  response.initFor(*message);

  if (!params.empty()) {
    scString qname;

    qname = params.getString(0);
    if (!qname.empty()) {
      if (!queueExists(qname))
      {
   	    res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Queue does not exists: ["+qname+"]");
        response.setError(error);
      } else { // no queue yet
        closeQueue(qname);
   	    res = SC_MSG_STATUS_OK;
      }
    } // qname filled
  } // has children

  return res;
}

int scJobManagerModule::handleCmdListQueues(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams();
  scString filter;

  if (params.empty())
    filter = "*";
  else
    filter = params.getString(0);

  listQueues(filter);
  res = SC_MSG_STATUS_OK;
  return res;
}

void scJobManagerModule::listQueues(const scString &filter)
{
  scDataNode listing;

  scString pname, targetAddr, returnAddr;

  scJobQueueTaskList::const_iterator p;

  scLog::addText("name|target|return");
  int cnt = 0;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    pname = (*p)->getQueueName();
    targetAddr = (*p)->getTargetAddr();
    returnAddr = (*p)->getReturnAddr();
    cnt++;
    scLog::addText(pname+"|"+targetAddr+"|"+returnAddr);
  }

  scLog::addText("--> "+toString(cnt)+" match(es) found");
}

void scJobManagerModule::closeAllQueues()
{
  scJobQueueTaskList::const_iterator p;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    (*p)->closeQueue();
  }
}

int scJobManagerModule::handleCmdStartJob(scMessage *message, scResponse &response)
{
  int res = forwardQueueMsg(message, response);
  return res;
}

int scJobManagerModule::handleCmdJobEnded(scMessage *message, scResponse &response)
{
  int res = forwardQueueMsg(message, response);
  return res;
}

int scJobManagerModule::forwardQueueMsg(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams();
  response.initFor(*message);

  if (!params.empty()) {
    scString qname;

    qname = params.getString("queue", "");
    if (!qname.empty()) {
      if (!queueExists(qname))
      {
   	    res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Queue does not exists: ["+qname+"]");
        response.setError(error);
      } else { // no queue yet
        scJobQueueTask *manager = checkQueue(qname);
        res = manager->handleQueueMsg(message, response);
      }
    } // qname filled
  } // has children

  return res;
}

int scJobManagerModule::forwardQueueMsgByJobId(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams();
  response.initFor(*message);

  if (!params.empty()) {
    scString qname;
    ulong64 jobId;

    if (params.hasChild("job_id"))
      jobId = params.getUInt64("job_id", 0);
    else
      jobId = params.getUInt64(0);

    qname = params.getString("queue", "");
    if (qname.empty())
      qname = getQueueForJobId(jobId);

    if (!qname.empty()) {
      if (!queueExists(qname))
      {
   	    res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Queue does not exists for: ["+toString(jobId)+"]");
        response.setError(error);
      } else { // no queue yet
        scJobQueueTask *manager = checkQueue(qname);
        res = manager->handleQueueMsg(message, response);
      }
    } // qname filled
  } // has children

  return res;
}

scString scJobManagerModule::getQueueForJobId(ulong64 jobId)
{
  scDataNode params;
  scDataNode resNode;
  scDataNode defValue("");

  params.addChild("job_id", new scDataNode(jobId));
  m_db->getValue("select queue from job where job_id = {job_id}", &params, resNode, &defValue);
  scString res = resNode.getAsString();
  return res;
}
