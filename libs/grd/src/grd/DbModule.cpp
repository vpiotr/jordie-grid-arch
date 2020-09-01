/////////////////////////////////////////////////////////////////////////////
// Name:        DbModule.cpp
// Project:     grdLib
// Purpose:     Database processing module - for remote SQL server support.
// Author:      Piotr Likus
// Modified by:
// Created:     20/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// boost
//#include <boost/algorithm/string.hpp>

// dtp
#include "dtp/dnode_serializer.h"

// sc
#include "base/rand.h"
#include "sc/utils.h"
#include "sc/db/DbSqlite.h"
#include "perf/Log.h"
#include "perf/time_utils.h"
//#include "sc/FileUtils.h"

// grd
#include "grd/DbModule.h"
#include "grd/DbProcEngine.h"
#include "grd/DbDataModule.h"
#include "grd/DbDataModuleSqlite.h"

using namespace dtp;

// ----------------------------------------------------------------------------
// local declarations
// ----------------------------------------------------------------------------

class grdDbConnectionTask: public scTask {
public:
    grdDbConnectionTask(grdDbModule *parent, const scString &cid);
    virtual ~grdDbConnectionTask();
    void setup(const scString &dbPath, const scString &procPath, bool procEnabled, ulong64 inactTimeout);
    void clearParent();
    virtual bool needsRun();
    virtual void intInit();
    virtual void intDispose(); 
    scString getCid() const;
    int handleConnMsg(scMessage *message, scResponse &response);
protected:
    //-- commands
    int handleCmdSqlExec(scMessage *message, scResponse &response);
    int handleCmdSqlSelect(scMessage *message, scResponse &response);
    int handleCmdSqlExecBatch(scMessage *message, scResponse &response);
    int handleCmdExecProc(scMessage *message, scResponse &response);
    int handleCmdSelectProc(scMessage *message, scResponse &response);
    int handleCmdRead(scMessage *message, scResponse &response);
    int handleCmdInsert(scMessage *message, scResponse &response);
    int handleCmdUpdate(scMessage *message, scResponse &response);
    int handleCmdDelete(scMessage *message, scResponse &response);
    int handleCmdBeginTrans(scMessage *message, scResponse &response);
    int handleCmdCommit(scMessage *message, scResponse &response);
    int handleCmdRollback(scMessage *message, scResponse &response);
    int handleCmdMetaList(scMessage *message, scResponse &response);
    int handleCmdMetaObjExists(scMessage *message, scResponse &response);
    int handleCmdEngineSupports(scMessage *message, scResponse &response);
    int handleCmdEngineName(scMessage *message, scResponse &response);
    int handleCmdLastInsId(scMessage *message, scResponse &response);
    //-- 
    virtual int intRun();
    void markLastContact();
    bool isInactTimeout();
    void prepareDatabase();
    void unprepareDatabase();
    void checkForTimeout();
    void parseDbPath(const scString &fullDbPath, scString &engineName, scString &intPath);
    void readSqlParams(const scDataNode &msgParams, const scString &paramName, scDataNode &output);
protected:
    scString m_cid;
    grdDbModule *m_parent;
    cpu_ticks m_lastContactDt;
    cpu_ticks m_inactTimeout;
    std::auto_ptr<grdDbDataModule> m_db;
    bool m_procEnabled;
    scString m_dbPath;
    scString m_procPath;
};

// ----------------------------------------------------------------------------
// private implementations
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// grdDbConnectionTask
// ----------------------------------------------------------------------------
grdDbConnectionTask::grdDbConnectionTask(grdDbModule *parent, const scString &cid): 
  m_parent(parent), m_cid(cid), m_inactTimeout(0)
{
    markLastContact();
}

grdDbConnectionTask::~grdDbConnectionTask()
{
    if (m_parent != SC_NULL)
        m_parent->clearTaskRef(this);
}

void grdDbConnectionTask::clearParent()
{
    m_parent = SC_NULL;
}

void grdDbConnectionTask::setup(const scString &dbPath, const scString &procPath, bool procEnabled, ulong64 inactTimeout)
{
    m_dbPath = dbPath;
    m_procPath = procPath;
    m_procEnabled = procEnabled;
    m_inactTimeout = inactTimeout;
}

scString grdDbConnectionTask::getCid() const
{
    return m_cid;
}

void grdDbConnectionTask::markLastContact()
{
    m_lastContactDt = cpu_time_ms(); 
}

bool grdDbConnectionTask::needsRun()
{
  return isInactTimeout();
} 

bool grdDbConnectionTask::isInactTimeout()
{
    return (m_inactTimeout > 0) && is_cpu_time_elapsed_ms(m_lastContactDt, m_inactTimeout);
}

void grdDbConnectionTask::intInit()
{
    prepareDatabase();
}

void grdDbConnectionTask::intDispose()
{
    unprepareDatabase();
}

int grdDbConnectionTask::intRun()
{
    int res = scTask::intRun();
    checkForTimeout();
    sleepFor(1000);
    res = res + 1;
    return res;
}

void grdDbConnectionTask::prepareDatabase()
{
    scString engineName;
    scString path = m_dbPath;
    
    parseDbPath(m_dbPath, engineName, path);
    if (engineName == DBENGINE_SQLITE)
        m_db.reset(new grdDbDataModuleSqlite());
    else 
        throw scError(scString("Unknown engine name: ")+engineName);

    scDataNode procList;
    m_parent->getProcDefList(procList);

    m_db->setup(m_dbPath, path, m_procPath, m_procEnabled, procList);
    m_db->init();
}

void grdDbConnectionTask::unprepareDatabase()
{
    if (m_db.get() != SC_NULL)
        m_db->dispose();
}

void grdDbConnectionTask::parseDbPath(const scString &fullDbPath, scString &engineName, scString &intPath)
{
    size_t idx = fullDbPath.find_first_of(':');
    if (idx == scString::npos)
        throw scError(scString("Incorrect db path format: ")+fullDbPath);
    engineName = fullDbPath.substr(0, idx);
    intPath = fullDbPath.substr(idx+1);
}

void grdDbConnectionTask::checkForTimeout()
{
    if (isInactTimeout()) 
        requestStop();
}

int grdDbConnectionTask::handleConnMsg(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "db") 
  {
    if (message->getCoreCommand() == "sql_exec") {
      res = handleCmdSqlExec(message, response);
    } else if (message->getCoreCommand() == "sql_select") {
      res = handleCmdSqlSelect(message, response);
    } else if (message->getCoreCommand() == "sql_exec_batch") {
      res = handleCmdSqlExecBatch(message, response);
    } else if (message->getCoreCommand() == "exec_proc") {
      res = handleCmdExecProc(message, response);
    } else if (message->getCoreCommand() == "select_proc") {
      res = handleCmdSelectProc(message, response);
    } else if (message->getCoreCommand() == "read") {
      res = handleCmdRead(message, response);
    } else if (message->getCoreCommand() == "insert") {
      res = handleCmdInsert(message, response);
    } else if (message->getCoreCommand() == "update") {
      res = handleCmdUpdate(message, response);
    } else if (message->getCoreCommand() == "delete") {
      res = handleCmdDelete(message, response);
    } else if (message->getCoreCommand() == "begin_trans") {
      res = handleCmdBeginTrans(message, response);
    } else if (message->getCoreCommand() == "commit") {
      res = handleCmdCommit(message, response);
    } else if (message->getCoreCommand() == "rollback") {
      res = handleCmdRollback(message, response);
    } else if (message->getCoreCommand() == "meta_list") {
      res = handleCmdMetaList(message, response);
    } else if (message->getCoreCommand() == "meta_obj_exists") {
      res = handleCmdMetaObjExists(message, response);
    } else if (message->getCoreCommand() == "engine_supports") {
      res = handleCmdEngineSupports(message, response);
    } else if (message->getCoreCommand() == "engine_name") {
      res = handleCmdEngineName(message, response);
    } else if (message->getCoreCommand() == "last_ins_id") {
      res = handleCmdLastInsId(message, response);
    }
  }

  return res;
}

void grdDbConnectionTask::readSqlParams(const scDataNode &msgParams, const scString &paramName, scDataNode &output)
{
    if (msgParams.hasChild(paramName)) {
        msgParams.getElement(paramName, output);
    }

    if (output.empty() && !output.isNull()) {
        dnSerializer serializer;
        scString paramsTxt = output.getAsString();
        serializer.convFromString(paramsTxt, output);
    }
}

int grdDbConnectionTask::handleCmdSqlExec(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("sql")) {
      scString sqlText = params.getString("sql");
      scDataNode sqlParams;

      readSqlParams(params, "db_params", sqlParams);

      uint rowsAffected = m_db->sqlExec(sqlText, sqlParams);

      scDataNode result(ict_parent);
      result.setElementSafe("affected", scDataNode(rowsAffected));

      response.setResult(result);

      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdSqlSelect(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("sql")) {
      scString sqlText = params.getString("sql");
      scDataNode sqlParams;

      readSqlParams(params, "db_params", sqlParams);

      uint limit = params.getUInt("limit", 0);
      ulong64 offset = params.getUInt64("offset", 0);

      scDataNode rows;
      m_db->sqlSelect(sqlText, sqlParams, limit, offset, rows);

      scDataNode result(ict_parent);
      result.setElementSafe("rows", scDataNode(rows));

      response.setResult(result);

      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdSqlExecBatch(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("sql")) {
      scString sqlText = params.getString("sql");
      scDataNode sqlParams;

      readSqlParams(params, "db_params", sqlParams);

      uint rowsAffected = m_db->sqlExec(sqlText, sqlParams);

      scDataNode result(ict_parent);
      result.setElementSafe("affected", scDataNode(rowsAffected));

      response.setResult(result);

      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdExecProc(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("name")) {
      scString procName = params.getString("name");
      scDataNode sqlParams;
      scDataNode procDef;

      readSqlParams(params, "db_params", sqlParams);

      uint execStatus = m_db->procExec(procName, sqlParams);

      scDataNode result(ict_parent);
      result.setElementSafe("exec_status", scDataNode(execStatus));

      response.setResult(result);

      if (execStatus == 0)
        res = SC_MSG_STATUS_OK;
      else
        res = SC_MSG_STATUS_ERROR;
  }

  return res;
}

int grdDbConnectionTask::handleCmdSelectProc(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("name")) {
      scString procName = params.getString("name");
      scDataNode sqlParams;
      scDataNode procDef;

      readSqlParams(params, "db_params", sqlParams);

      scDataNode rows;
      uint execStatus = m_db->procSelect(procName, sqlParams, rows);

      scDataNode result(ict_parent);
      result.setElementSafe("exec_status", scDataNode(execStatus));
      result.setElementSafe("rows", scDataNode(rows));

      response.setResult(result);

      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdRead(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("oname")) {
      scString objName = params.getString("oname");
      scDataNode filter;
      scDataNode order;
      scDataNode columns;

      readSqlParams(params, "filter", filter);
      readSqlParams(params, "order", order);
      uint limit = params.getUInt("limit", 0);
      ulong64 offset = params.getUInt64("offset", 0);

      if (params.hasChild("columns"))
          params.getElement("columns", columns);
 
      scDataNode rows;
      m_db->readRows(objName, columns, filter, order, limit, offset, rows);

      scDataNode result(ict_parent);
      result.setElementSafe("rows", scDataNode(rows));

      response.setResult(result);

      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdInsert(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("oname") && params.hasChild("values")) {
      scString objName = params.getString("oname");
      scDataNode sqlParams;
      scDataNode values;

      params.getElement("values", values);
      if (!values.empty()) {

          m_db->insertRows(objName, values);

          res = SC_MSG_STATUS_OK;
      }
  }

  return res;
}

int grdDbConnectionTask::handleCmdUpdate(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("oname") && params.hasChild("values")) {
      scString objName = params.getString("oname");
      scDataNode filter;
      scDataNode values;

      params.getElement("values", values);
      readSqlParams(params, "filter", filter);

      if (!values.empty() && !filter.empty()) {
          uint rowsAffected = m_db->updateRows(objName, filter, values);

          scDataNode result(ict_parent);
          result.setElementSafe("affected", scDataNode(rowsAffected));

          response.setResult(result);

          res = SC_MSG_STATUS_OK;
      }
  }

  return res;
}

int grdDbConnectionTask::handleCmdDelete(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("oname")) {
      scString objName = params.getString("oname");
      scDataNode filter;
      scDataNode values;

      readSqlParams(params, "filter", filter);

      if (!filter.empty()) {
        uint rowsAffected = m_db->deleteRows(objName, filter);

        scDataNode result(ict_parent);
        result.setElementSafe("affected", scDataNode(rowsAffected));

        response.setResult(result);

        res = SC_MSG_STATUS_OK;
      }
  }

  return res;
}

int grdDbConnectionTask::handleCmdBeginTrans(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  m_db->startTrans();
  res = SC_MSG_STATUS_OK;

  return res;
}

int grdDbConnectionTask::handleCmdCommit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  m_db->commit();
  res = SC_MSG_STATUS_OK;

  return res;
}

int grdDbConnectionTask::handleCmdRollback(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  m_db->rollback();
  res = SC_MSG_STATUS_OK;

  return res;
}

int grdDbConnectionTask::handleCmdMetaList(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  if (params.hasChild("obj_type")) {
      scString objType = params.getString("obj_type");

      scDataNode result;
      m_db->getMetaObjList(objType, result);
      response.setResult(result);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdMetaObjExists(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  if (params.hasChild("obj_type") && params.hasChild("obj_name")) {
      scString objType = params.getString("obj_type");
      scString objName = params.getString("obj_name");

      scDataNode result(m_db->getMetaObjExists(objType, objName));
      
      response.setResult(result);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdEngineSupports(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  if (params.hasChild("flags") && params.hasChild("domain")) {
      uint flags = params.getUInt("flags");
      uint domain = params.getUInt("domain", 0);

      scDataNode result(m_db->getEngineSupports(flags, domain));
      
      response.setResult(result);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdDbConnectionTask::handleCmdEngineName(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  //scDataNode result("name", m_db->getEngineName());
  scDataNode result(m_db->getEngineName());
      
  response.setResult(result);
  res = SC_MSG_STATUS_OK;

  return res;
}

int grdDbConnectionTask::handleCmdLastInsId(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  //scDataNode result("last_ins_id", m_db->getLastInsId());
  scDataNode result(m_db->getLastInsId());
      
  response.setResult(result);
  res = SC_MSG_STATUS_OK;

  return res;
}

// ----------------------------------------------------------------------------
// public implementations
// ----------------------------------------------------------------------------
grdDbModule::grdDbModule(): scModule() {
    m_inactTimeout = 0;
    m_dbDefs = scDataNode(ict_parent);
    m_procDefs = scDataNode(ict_parent);
    m_nextCid = randomUInt(1, 100000);
}

grdDbModule::~grdDbModule() 
{
    clearConnLinks();
}

ulong64 grdDbModule::genNextCid()
{
    ulong64 res = m_nextCid;
    m_nextCid++;
    return res;
}

void grdDbModule::clearConnLinks()
{
  grdDbConnectionTask *task = SC_NULL;
  scString pname;
  
  grdDbTaskPtrList::iterator p;

  for (p = m_connections.begin(); p != m_connections.end(); ++p) {
    task = dynamic_cast<grdDbConnectionTask *>(*p);
    task->clearParent();
  }  
}

void grdDbModule::clearTaskRef(scTask *ref)
{
  grdDbTaskPtrList::iterator p;

  for (p = m_connections.begin(); p != m_connections.end(); ++p) {
    if (*p == ref) {
        m_connections.erase(p);
        break;
    }
  }  
}

int grdDbModule::handleMessage(scMessage *message, scResponse &response)
{
 
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "db") 
  {
    if (!isModuleMessage(message->getCoreCommand())) {
      res = forwardMsgByCid(message, response);
    } else if (message->getCoreCommand() == "init") {
      res = handleCmdInit(message, response);
    } else if (message->getCoreCommand() == "define_db") {
      res = handleCmdDefineDb(message, response);
    } else if (message->getCoreCommand() == "define_proc") {
      res = handleCmdDefineProc(message, response);
    } else if (message->getCoreCommand() == "open") {
      res = handleCmdOpen(message, response);
    } else if (message->getCoreCommand() == "close") {
      res = handleCmdClose(message, response);
    } 
  }

  return res;
}

scTaskIntf *grdDbModule::prepareTaskForMessage(scMessage *message)
{
  scTask *res = SC_NULL;
  scString coreCmd = message->getCoreCommand();
  
  if (
     (message->getInterface() == "db")
     )
  {   
  //init, listen, close, clear, get_status
    //if (coreCmd == "open")
    //{
    //  res = prepareConnectionManager(message);
    //}  
  }  
  
  return res;
}

scTask *grdDbModule::prepareConnectionManager(scMessage *message, const scString &cid)
{
  std::auto_ptr<grdDbConnectionTask> res;

  scDataNode params = message->getParams(); 
  
  if (!params.empty()) {
    scString dbName = params.getString("db_name", "");
    scDataNode &definedParams = m_dbDefs[dbName];
    scString dbPath = definedParams.getString("path", "");
    scString procDir = definedParams.getString("proc_dir", "");
    bool procEnabled = definedParams.getBool("proc_enabled", false);

    if (!dbName.empty()) {
      res.reset(new grdDbConnectionTask(this, cid));
      res->setup(dbPath, procDir, procEnabled, m_inactTimeout);
      m_connections.push_back(res.get());
    }
  }  
  
  return res.release();
}

scStringList grdDbModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("db");  
  return res;
}

bool grdDbModule::isModuleMessage(const scString &coreCmd)
{
    const char* byIdCmds = "|init|define_db|define_proc|open|close|";
    scString wrk(byIdCmds);
    if (wrk.find(scString("|")+coreCmd+"|") != scString::npos)
        return true;
    else
        return false;
}

int grdDbModule::forwardMsgByCid(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString cid;
    
    cid = params.getString("cid", "");
    if (!cid.empty()) {
      if (!connectionExists(cid)) 
      {
   	res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Connection does not exists: ["+cid+"]");
        response.setError(error);   	    
      } else { // connection found
        grdDbConnectionTask *task = dynamic_cast<grdDbConnectionTask *>(checkConnectionTask(cid));
        res = task->handleConnMsg(message, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int grdDbModule::handleCmdInit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams(); 

  if (!params.empty())
  {
    ulong64 inactTimeout = params.getUInt64("inact_timeout", 0);
    this->setup(inactTimeout);
    res = SC_MSG_STATUS_OK;                
  }     
  return res;
}

int grdDbModule::handleCmdDefineDb(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams(); 

  if (params.hasChild("alias"))
  {
    scString alias = params.getString("alias", "");
    scString dbPath = params.getString("path", "");
    bool procEnabled = params.getBool("proc_enabled", false);
    scString procDir = params.getString("proc_dir", "");
    if (performDefineDb(alias, dbPath, procEnabled, procDir))
      res = SC_MSG_STATUS_OK;                
  }     
  return res;
}

int grdDbModule::handleCmdDefineProc(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams(); 

  if (params.hasChild("alias"))
  {
    scString alias = params.getString("alias", "");
    scString procPath = params.getString("path", "");
    if (performDefineProc(alias, procPath))
      res = SC_MSG_STATUS_OK;                
  }     
  return res;
}

int grdDbModule::handleCmdOpen(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (params.hasChild("db_name")) {
    scString dbName;
    
    dbName = params.getString("db_name");
    if (!dbName.empty()) {
      if (!m_dbDefs.hasChild(dbName)) 
      {
   	res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Unknown database name: ["+dbName+"]");
        response.setError(error);   	    
      } else { // database ready to be connected
   	res = SC_MSG_STATUS_OK;        
        scDataNode result(ict_parent);

        result.setElementSafe("cid", scDataNode(toString(genNextCid())));
        response.setResult(result);

        std::auto_ptr<scTask> newTask(prepareConnectionManager(message, result.getString("cid")));
        getScheduler()->addTask(newTask.release());
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int grdDbModule::handleCmdClose(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (params.hasChild("cid")) {
    scString cid;
    
    cid = params.getString("cid");
    if (!cid.empty()) {
      grdDbConnectionTask *task = dynamic_cast<grdDbConnectionTask *>(findConnectionTask(cid));
      if (task == SC_NULL) 
      {
   	res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Wrong cid: ["+cid+"]");
        response.setError(error);   	    
      } else { 
        task->requestStop();
   	res = SC_MSG_STATUS_OK;        
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

void grdDbModule::setup(ulong64 inactTimeout)
{
    m_inactTimeout = inactTimeout;
}

bool grdDbModule::performDefineDb(const scString &alias, const scString &dbPath, bool procEnabled, const scString &procDir)
{
    scDataNode dbDef(ict_parent);
    dbDef.setElementSafe("path", scDataNode(dbPath));
    dbDef.setElementSafe("proc_enabled", scDataNode(procEnabled));
    if (!procDir.empty())
      dbDef.setElementSafe("proc_dir", scDataNode(procDir));
    m_dbDefs.setElementSafe(alias, dbDef);
    return true;
}

bool grdDbModule::performDefineProc(const scString &alias, const scString &procPath)
{
    scDataNode procDef(ict_parent);
    procDef.setElementSafe("path", scDataNode(procPath));
    m_procDefs.setElementSafe(alias, procDef);
    return true;
}

bool grdDbModule::getProcDef(const scString &alias, scDataNode &output)
{
    bool res = m_procDefs.hasChild(alias);
    if (res)
        m_procDefs.getElement(alias, output);
    else
        output.clear();
    return res;
}

void grdDbModule::getProcDefList(scDataNode &output)
{
    output = m_procDefs;
}

bool grdDbModule::connectionExists(const scString &cid)
{
  return (findConnectionTask(cid) != SC_NULL);
}

scTask *grdDbModule::findConnectionTask(const scString &cid)
{
  scTask *res = SC_NULL;
  scString pname;
  
  grdDbTaskPtrList::const_iterator p;

  for (p = m_connections.begin(); p != m_connections.end(); ++p) {
    pname = dynamic_cast<grdDbConnectionTask *>(*p)->getCid();
    if (pname == cid)
    {
      res = dynamic_cast<grdDbConnectionTask *>(*p);
      break;
    }  
  }  
  return res;       
}

scTask *grdDbModule::checkConnectionTask(const scString &cid)
{
    scTask *res = findConnectionTask(cid);
    if (res == SC_NULL)
        throw scError(scString("Unknown connection, id=[")+cid+"]");
    return res;
}
