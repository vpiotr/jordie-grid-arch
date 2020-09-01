/////////////////////////////////////////////////////////////////////////////
// Name:        PersQueue.cpp
// Project:     grdLib
// Purpose:     Transactional queue which stores messages in database.
// Author:      Piotr Likus
// Modified by:
// Created:     05/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// std
#include <set>

// boost
#include <boost/filesystem.hpp> 

// dtp
#include "dtp/dnode_serializer.h"

// sc
#include "sc/log.h"
#include "sc/FileUtils.h"

// grd
#include "grd/PersQueue.h"

using namespace dtp;

const uint PQ_DEF_ERROR_DELAY = 10*1000;
const uint PQ_DEF_PURGE_INTERVAL = 30*60*1000;
const uint PQ_DEF_STORAGE_TIMEOUT = 60*60*1000;
const uint PQ_DEF_HANDLE_TIMEOUT = 2*60*1000;
const uint PQ_DEF_REPLY_TIMEOUT = 5*60*1000;
const uint PQ_DEF_STATUS_CHK_DELAY = 100;
const uint PQ_LOCK_SAVE_FREQ = 1000;
const char *PQ_QMAN_TASK_NAME_PFX = "pqueuem_";

// ----------------------------------------------------------------------------
// local declarations
// ----------------------------------------------------------------------------
class grdPersQueueMsgList
{
public:
    grdPersQueueMsgList() {}
    virtual ~grdPersQueueMsgList() {}
    scDataNode *newMessage(uint msgId) { return new scDataNode(toString(msgId)); }
    void insert(scDataNode *msgRec) { m_list.addChild(msgRec); }
    void erase(ulong64 msgId) { 
        scDataNode::size_type idx = m_list.indexOfName(getKeyName(msgId));
        if (idx != scDataNode::npos)
          m_list.eraseElement(idx);
    }
    scString getKeyName(ulong64 msgId)
    {
        return toString(msgId);
    }
    bool hasItem(const scString &keyName) { return m_list.hasChild(keyName); }
    uint getItemStatus(const scString &keyName) { return m_list[keyName].getUInt("status"); }
    grdPersQueueMsgStatus getItemStatusAt(uint itemIndex) { return static_cast<grdPersQueueMsgStatus>(m_list[itemIndex].getUInt("status")); }
    scDateTime getItemUpdatedDtAt(uint itemIndex) { 
       return m_list[itemIndex].getDateTime("updated_dt", 0.0); 
    }

    uint getItemIndex(ulong64 itemId) {
        scDataNode::size_type idx = m_list.indexOfName(toString(itemId));
        if (idx != scDataNode::npos)
          return idx;
        else
          return m_list.size();
    }

    scDateTime getItemAddedDtAt(uint itemIndex) { 
       return m_list[itemIndex].getDateTime("added_dt", 0.0); 
    }

    size_t getItemCountByStatus(uint status) {
       size_t res = 0;
        for(uint i=0, epos = m_list.size(); i != epos; i++) {
            if (getItemStatusAt(i) == status) { 
                res++;
                break;
            }
        }
        return res;
    }

    bool getItem(ulong64 itemId, scDataNode &output) { 
        scString keyName = getKeyName(itemId);
        bool res = hasItem(keyName);
        if (res) output = m_list[keyName]; 
        else output.clear();
        return res;
    }
    scDataNode getItemAt(size_t itemIndex) const { 
        scDataNode res = m_list[itemIndex];
        return res;
    }
    ulong64 getItemIdAt(uint itemIndex) { return m_list[itemIndex].getUInt64("message_id"); }

    void setStatusAndLock(ulong64 itemId, uint status, ulong64 lockId, scDateTime changeDt)
    {
        scDataNode &ref = m_list[getKeyName(itemId)];
        ref.setUInt("status", status);
        ref.setElementSafe("lock_id", scDataNode(lockId));
        ref.setElementSafe("updated_dt", scDataNode(changeDt));
    }
    void setStatus(ulong64 itemId, uint status, scDateTime changeDt)
    {
        scDataNode &ref = m_list[getKeyName(itemId)];
        ref.setUInt("status", status);
        ref.setElementSafe("updated_dt", scDataNode(changeDt));
    }
    size_t size() { return m_list.size(); }
    void getItemAt(size_t index, scDataNode &output) {
        m_list.getElement(index, output);
    }
    size_t getItemSizeAt(size_t index) {
      scDataNode &msgRec = m_list[index];

      return 
            msgRec.getString("command", "").length()+
            msgRec.getString("params", "").length()+
            msgRec.getString("result", "").length()+
            msgRec.getString("error", "").length()+
            msgRec.getString("reference", "").length();
    }
    bool findReadyMessage(ulong64 &msgId) {
        bool res = false;
        for(uint i=0, epos = m_list.size(); i != epos; i++) {
            if (getItemStatusAt(i) == pqsReady) { 
                res = true;
                msgId = m_list[i].getUInt64("message_id");
                break;
            }
        }
        return res;
    }

    bool findByRef(const scString &ref, ulong64 &outputMsgId) 
    {
        bool res = false;
        for(uint i=0, epos = m_list.size(); i != epos; i++) {
            if (m_list[i].getString("reference", "") == ref) { 
                res = true;
                outputMsgId = m_list[i].getUInt64("message_id");
                break;
            }
        }
        return res;
    }

    void throwUnknownItem(ulong64 itemId) {
        throw scError(scString("Unknown item id: ")+toString(itemId));
    }

    void setItemExecResult(ulong64 itemId, int replyStatus, const scDataNode &result, const scDataNode &error)
    {
        scString keyName = getKeyName(itemId);
        bool found = hasItem(keyName);
        if (!found)
            throwUnknownItem(itemId);
        m_list[keyName].setElementSafe("exec_status", scDataNode(replyStatus));
        m_list[keyName].setElementSafe("result", result);
        m_list[keyName].setElementSafe("error", error);
    }

    void setItemList(const scDataNode &list) {
        m_list = list;
    }

    bool lockMessageList(const scDataNode &msgList, scDateTime when)
    {
        bool res = false;
        ulong64 msgId;
        int itemIndex;

        for(uint i=0, epos = msgList.size(); i != epos; i++) {
            msgId = msgList[i].getUInt64("message_id");
            itemIndex = getItemIndex(msgId);
            if (itemIndex >= m_list.size())
                continue;

            if (m_list[itemIndex].getUInt64("lock_id", 0) > 0) 
                continue;

            if (getItemStatusAt(itemIndex) == pqsLocked) 
                continue;
            
            m_list[itemIndex].setElementSafe("old_status", m_list[itemIndex].getElement("status"));
            m_list[itemIndex].setElementSafe("status", scDataNode(static_cast<uint>(pqsLocked)));
            m_list[itemIndex].setElementSafe("updated_dt", scDataNode(when));
            res = true;
        }
        return res;
    }

    bool unlockMessageList(const scDataNode &msgList, scDateTime when)
    {
        bool res = false;
        ulong64 msgId;
        int itemIndex;

        for(uint i=0, epos = msgList.size(); i != epos; i++) {
            msgId = msgList[i].getUInt64("message_id");
            itemIndex = getItemIndex(msgId);
            if (itemIndex >= m_list.size())
                continue;

            if (getItemStatusAt(itemIndex) != pqsLocked) 
                continue;
            
            m_list[itemIndex].setElementSafe("status", m_list[itemIndex].getElement("old_status"));
            m_list[itemIndex].setElementSafe("updated_dt", scDataNode(when));
            res = true;
        }
        return res;
    }

protected:
    scDataNode getItemRef(ulong64 msgId) { return m_list[getKeyName(msgId)]; }
protected:
    scDataNode m_list;
};

class grdPersQueueTask: public scTask {
public:
    grdPersQueueTask(grdPersQueueModule *parent, const scString &queueName);
    virtual ~grdPersQueueTask();
    void clearParent();
    scString getQueueName();
    int handleQueueMsg(scMessage *message, scResponse &response);
    void setup(const scString &execAddr, const scString &replyToAddr, const scString &errorAddr, 
        uint execLimit, uint errorLimit, uint errorDelay, uint purgeInterval, 
        uint storageTimeout, uint handleTimeout, uint replyTimeout, const scString &archiveFName, const scString &replyCmd);
    virtual int handleMessage(scEnvelope &envelope, scResponse &response);    
    void handleSubmitError(ulong64 msgId, uint lockId, const scString &errorMsg);
    void handleSubmitResultOK(ulong64 msgId, uint lockId, const scMessage &message, const scResponse &a_response);
    void handleReplyError(ulong64 msgId, uint lockId, const scString &errorMsg);
    void handleReplyResultOK(ulong64 msgId, uint lockId, const scMessage &message);
    virtual bool needsRun();
    virtual void intInit();
    uint getMessageCount();
protected:
    virtual int intRun();
    //-- commands
    int handleCmdPut(scMessage *message, scResponse &response);
    int handleCmdMList(scMessage *message, scResponse &response);
    int handleCmdFetch(scMessage *message, scResponse &response);
    int handleCmdHandled(scMessage *message, scResponse &response);
    int handleCmdLock(scMessage *message, scResponse &response);
    int handleCmdUnlock(scMessage *message, scResponse &response);
    int handleCmdCancel(scMessage *message, scResponse &response);
    int handleCmdPeek(scMessage *message, scResponse &response);
    int handleCmdExport(scMessage *message, scResponse &response);
    int handleCmdImport(scMessage *message, scResponse &response);
    int handleCmdRegister(scMessage *message, scResponse &response);
    int handleCmdPurge(scMessage *message, scResponse &response);
    //-- 
    void addMessageList(const scDataNode &msgList);
    ulong64 addMessage(const scDataNode &msgRec);
    ulong64 putMessage(scMessage *message);
    grdPersQueueDataModule *checkDataModule();
    ulong64 insertMsgToDb(const scDataNode &msgRec, uint status);
    void insertMsgToMemQueue(ulong64 msgId, const scDataNode &msgRec, uint status);
    void checkExecuteMsg(ulong64 msgId);
    void executeMsg(ulong64 msgId);
    grdPersQueueModule *checkParent();
    scString getExecAddr() const;
    scString getReplyToAddr() const;
    ulong64 genNewLockId();
    void changeMsgStatusAndLock(ulong64 msgId, grdPersQueueMsgStatus status, ulong64 lockId);
    void changeMsgStatusAndLock(ulong64 msgId, grdPersQueueMsgStatus status, ulong64 lockId, scDateTime nowDt);
    bool changeMsgStatusAndUnlock(ulong64 msgId, grdPersQueueMsgStatus status, ulong64 lockId);
    ulong64 getQueueId();
    void logWarning(ulong64 msgId, const scString &msgText);
    void logError(ulong64 msgId, const scString &msgText);
    void logInfo(ulong64 msgId, const scString &msgText);
    void performReply(ulong64 msgId);
    void changeMsgStatus(ulong64 msgId, grdPersQueueMsgStatus status);
    void getMessageList(scDataNode &output);
    void fetchMessageList(uint limit, scDataNode &output);
    bool fetchMessage(scDataNode &output, ulong64 &lockId);
    bool setMsgExecError(ulong64 msgId, uint lockId, const scString &errorMsg);
    bool setMsgExecError(ulong64 msgId, uint lockId, const scDataNode &error);
    bool setMsgExecSuccess(ulong64 msgId, uint lockId, uint resultStatus, const scDataNode &result);
    void processHandledMsg(ulong64 msgId);
    bool performMsgHandled(ulong64 msgId, ulong64 lockId, int replyStatus, const scDataNode &result, const scDataNode &error);
    void loadMessages();
    void restartMessages();
    uint getActiveMsgCount();
    bool canExecuteAnyMsg();
    void scanForExecute();
    bool isTimeForStatusCheck();
    void checkScanStatusesNeeded();
    void performStatusCheck();
    void handleExecEnd();
    void scanForRetryErrors();
    void scanForExecTimeout();
    void scanForStorageTimeout();
    void prepareMsgForPurge(ulong64 msgId);
    void prepareMsgForPurge(ulong64 msgId, scDateTime chgTime);    
    void initNextLockId();
    void checkPurgeNeeded();
    bool lockForPurge(ulong64 lockId);
    void performPurgeDelOnly();
    bool isTimeForPurge();
    void performPurgeWithArchive();
    bool useArchiveForPurge();
    scString findFreeFileName(const scString &fnameTpl, const scString &varPart);
    void saveArchive(const scString &fnameTpl, const scDataNode &rows);
    void saveArchiveToFile(const scString &fpath, const scDataNode &rows);
    void reformatArchiveRows(const scDataNode &rows, scDataNode &output);
    void performPurge();
    void lockMessageList(const scDataNode &msgList);
    void unlockMessageList(const scDataNode &msgList);
    void cancelMessageList(const scDataNode &msgList);
    bool cancelMessage(ulong64 msgId);
    bool peekMessage(ulong64 msgId, scDataNode &output);
    bool peekMessage(const scString &ref, scDataNode &output);
    void exportMessages(const scString &fpath, grdPersQueueMsgStatus status, ulong64 msgId);
    void importMessages(const scString &fpath);
    void registerQueue(const scString &aliasName, const scString &execAtAddr);
protected:
    grdPersQueueModule *m_parent;
    scString m_queueName;
    // config
    uint m_execLimit;
    uint m_errorLimit;
    uint m_errorDelay;
    uint m_purgeInterval;
    uint m_storageTimeout;
    uint m_handleTimeout;
    uint m_replyTimeout;
    ulong64 m_nextLockId;
    ulong64 m_queueId;
    scString m_execAddr;
    scString m_replyToAddr;
    scString m_errorAddr;
    scString m_archiveFName;
    scString m_replyCmd;
    grdPersQueueMsgList m_msgList;
    cpu_ticks m_lastStatusCheck;
    cpu_ticks m_statusCheckDelay;
    cpu_ticks m_lastPurge;
};

class grdPersQueueExecHandler: public scRequestHandler {
public:
  grdPersQueueExecHandler(grdPersQueueTask *sender, ulong64 msgId, uint lockId): scRequestHandler() 
    {m_sender = sender; m_msgId = msgId; m_lockId = lockId;}
  virtual ~grdPersQueueExecHandler() {};
  virtual void handleCommError(const scString &errorText, RequestPhase phase);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
protected:
  grdPersQueueTask *m_sender;  
  ulong64 m_msgId;
  uint m_lockId;
};

class grdPersQueueReplyHandler: public scRequestHandler {
public:
  grdPersQueueReplyHandler(grdPersQueueTask *sender, ulong64 msgId, uint lockId = 0): scRequestHandler() 
    {m_sender = sender; m_msgId = msgId; m_lockId = lockId;}
  virtual ~grdPersQueueReplyHandler() {};
  virtual void handleCommError(const scString &errorText, RequestPhase phase);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
protected:
  grdPersQueueTask *m_sender;  
  ulong64 m_msgId;
  uint m_lockId;
};

// ----------------------------------------------------------------------------
// private implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// grdPersQueueExecHandler
// ----------------------------------------------------------------------------
void grdPersQueueExecHandler::handleCommError(const scString &errorText, RequestPhase phase)
{
  m_sender->handleSubmitError(m_msgId, m_lockId, errorText);
}

void grdPersQueueExecHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  m_sender->handleSubmitResultOK(m_msgId, m_lockId, a_message, a_response);
}

void grdPersQueueExecHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  scString msg = toString(a_response.getStatus())+": "+a_response.getError().getString("text", "");
  m_sender->handleSubmitError(m_msgId, m_lockId, msg);
}

// ----------------------------------------------------------------------------
// grdPersQueueReplyHandler
// ----------------------------------------------------------------------------
void grdPersQueueReplyHandler::handleCommError(const scString &errorText, RequestPhase phase)
{
  m_sender->handleReplyError(m_msgId, m_lockId, errorText);
}

void grdPersQueueReplyHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  m_sender->handleReplyResultOK(m_msgId, m_lockId, a_message);
}

void grdPersQueueReplyHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  scString msg = toString(a_response.getStatus())+": "+a_response.getError().getString("text", "");
  m_sender->handleReplyError(m_msgId, m_lockId, msg);
}


// ----------------------------------------------------------------------------
// public implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// grdPersQueueDataModule
// ----------------------------------------------------------------------------
grdPersQueueDataModule::grdPersQueueDataModule(scDbBase *db): m_db(db)
{
}

grdPersQueueDataModule::~grdPersQueueDataModule()
{
}

bool grdPersQueueDataModule::isSchemaReady()
{
  return m_db->tableExists("queue");
}

void grdPersQueueDataModule::initSchema()
{
  if (!isSchemaReady())
    prepareDbStructure(); 
}

void grdPersQueueDataModule::prepareDbStructure()
{ 
  scString sql =
  scString(
  "create table queue(queue_id integer primary key, name varchar(32), added_dt DATETIME, updated_dt DATETIME, next_lock_id integer);"
  "CREATE UNIQUE INDEX queue_ix1 ON queue(name);"
  "create table queue_config(queue_id integer primary key, param_name varchar(32), param_value varchar(1024), added_dt DATETIME, updated_dt DATETIME);"
  "CREATE UNIQUE INDEX queue_config_ix1 ON queue_config(queue_id, param_name);"
  );

  executeMultiSql(sql);
}

void grdPersQueueDataModule::executeMultiSql(const scString &sqlText)
{
  scDataNode sqlList = scDataNode::explode(";", sqlText);
  scString cmd;
  
  for(int i=0, epos=sqlList.size(); i!=epos; i++)
  {
    cmd = sqlList.getString(i);
    if (strTrim(cmd).length() > 0)
      m_db->execute(cmd);  
  } 
}

// returns id of queue
ulong64 grdPersQueueDataModule::initQueue(const scString &qname, bool noData)
{
    if (!noData) {
        scString tabName = getQueueMsgTableName(qname);
        if (!m_db->tableExists(tabName))
            addQueueMsgTable(qname);
    }

    ulong64 res;

    if (!getQueueId(qname, res))
    {
        scDataNode params(ict_parent);
        params.addElement("queue", scDataNode(qname));

        scString insertSql = 
            "insert into queue(name, added_dt) values({queue}, {added_dt})";

        params.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));
        m_db->execute(insertSql, &params);
        res = m_db->getLastInsertedId();
    } 

    return res;
}

bool grdPersQueueDataModule::recordExists(const scString &sql, scDataNode *params)
{
   scDataNode tmpRow;
   return m_db->getRow(sql, params, tmpRow);
}

bool grdPersQueueDataModule::getQueueId(const scString &queueName, ulong64 &qid)
{
    scString sql =
        "select queue_id from queue where name = {queue}";

    scDataNode params(ict_parent);
    params.addElement("queue", scDataNode(queueName));

    scDataNode valueNode;

    bool res = m_db->getValue(sql, &params, valueNode);

    if (res)
        qid = valueNode.getAsUInt();
    else
        qid = 0;

    return res;
}

ulong64 grdPersQueueDataModule::checkQueueId(const scString &queueName)
{
    ulong64 res;
    if (!getQueueId(queueName, res)) {
        throw scError(scString("Queue [")+queueName+"] does not exits");
    }
    return res;
}

scString grdPersQueueDataModule::getQueueMsgTableName(const scString &qname)
{
    return scString("message_")+qname;
}

void grdPersQueueDataModule::addQueueMsgTable(const scString &qname)
{
  scString sql =
  scString(
  "create table message_{queue}(message_id integer primary key, status integer, added_dt DATETIME, updated_dt DATETIME, command varchar(32), params varchar(1024), "
  "exec_status integer, error varchar(1024), result varchar(1024), "
  "reply_cmd varchar(32), reference varchar(1024), error_cnt integer, "
  "lock_id integer, "
  "old_status integer "
  ");"
  );

  scDataNode params(ict_parent);
  params.addElement("queue", scDataNode(qname));

  executeMultiSql(fillTemplateValues(sql, params));
}

void grdPersQueueDataModule::defineQueue(const scString &qname, const scDataNode &params)
{
    uint qid = checkQueueId(qname);

    deleteQueueConfig(qid);

    for(uint i=0, epos = params.size(); i != epos; i++)
        defineQueueParam(qid, params.getElement(i), params.getElementName(i));
}

void grdPersQueueDataModule::defineQueueParam(uint queueId, const scDataNode &param, const scString &paramName)
{
    scString insertSql = 
        "insert into queue_config(queue_id, param_name, param_value, added_dt) "
        "values({queue_id}, {param_name}, {param_value}, {added_dt})";

    scDataNode sqlParams(ict_parent);
    sqlParams.addChild("queue_id", new scDataNode(queueId));
    sqlParams.addChild("param_name", new scDataNode(paramName));
    sqlParams.addChild("param_value", new scDataNode(param));
    sqlParams.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));
    m_db->execute(insertSql, &sqlParams);
}

bool grdPersQueueDataModule::undefineQueue(const scString &qname)
{
    uint qid = checkQueueId(qname);
    return deleteQueueConfig(qid);
}

bool grdPersQueueDataModule::deleteQueueConfig(uint queueId)
{
    scString clearSql = 
        "delete from queue_config where queue_id = {queue_id}";

    scDataNode sqlParams(ict_parent);
    sqlParams.addChild("queue_id", new scDataNode(queueId));
    m_db->execute(clearSql, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

bool grdPersQueueDataModule::deleteQueue(uint queueId)
{
    scString clearSql = 
        "delete from queue where queue_id = {queue_id}";

    scDataNode sqlParams(ict_parent);
    sqlParams.addChild("queue_id", new scDataNode(queueId));
    m_db->execute(clearSql, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

bool grdPersQueueDataModule::dropQueueMsgTable(const scString &queueName)
{
    scString clearSql = 
        "drop table if exists {table_name}";

    scDataNode sqlParams(ict_parent);
    sqlParams.addChild("table_name", new scDataNode(getQueueMsgTableName(queueName)));
    m_db->execute(fillTemplateValues(clearSql, sqlParams));

    return true;
}

// remove all information about a given queue
bool grdPersQueueDataModule::dropQueue(const scString &qname)
{
    bool res = false;
    bool operRes;

    scString tabName = getQueueMsgTableName(qname);
    if (m_db->tableExists(tabName)) {
        operRes = dropQueueMsgTable(qname);
        res = res || operRes;
    }

    ulong64 qid;
    bool qidFound = getQueueId(qname, qid);
    if (qidFound) {
      operRes = deleteQueueConfig(qid);
      res = res || operRes;
      operRes =  deleteQueue(qid);
      res = res || operRes;
    }

    return res;
}

ulong64 grdPersQueueDataModule::insertMessage(const scString &queueName, const scString &command, const scDataNode &params, const scDataNode &ref, 
    const scString &replyCmd, uint status)
{
   scString tabName = getQueueMsgTableName(queueName);
   return insertMessageToTab(tabName, command, params, ref, replyCmd, status);
}

ulong64 grdPersQueueDataModule::insertMessageToTab(const scString &tabName, const scString &command, const scDataNode &params, 
    const scDataNode &ref, const scString &replyCmd, uint status)
{
    dnSerializer serializer;
    scDataNode sqlParams(ict_parent);
    scString paramsTxt;
    scString refTxt;
    if (!params.isNull()) {
        if (params.isContainer())
          serializer.convToString(params, paramsTxt);
        else
          paramsTxt = params.getAsString();
    }
    if (!ref.isNull()) {
        if (ref.isContainer())
           serializer.convToString(ref, refTxt);
        else
           refTxt = ref.getAsString();
    }
    sqlParams.addElement("command", scDataNode(command));
    sqlParams.addElement("params", scDataNode(paramsTxt));
    sqlParams.addElement("reference", scDataNode(refTxt));
    sqlParams.addElement("reply_cmd", scDataNode(replyCmd));
    sqlParams.addElement("status", scDataNode(static_cast<uint>(status)));
    sqlParams.addChild(scDbBase::newExpr("added_dt", "datetime('now')"));

    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlText = "insert into {table_name}(status, added_dt, command, params, reference, reply_cmd, error_cnt, lock_id) "
        "values({status}, {added_dt}, {command}, {params}, {reference}, {reply_cmd}, 0, 0)";

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return m_db->getLastInsertedId();
}

void grdPersQueueDataModule::setMsgStatus(scString &queueName, ulong64 msgId, uint status)
{
    setMsgStatusAndLock(queueName, msgId, status, 0);
}

void grdPersQueueDataModule::setMsgStatusAndLock(scString &queueName, ulong64 msgId, uint status, ulong64 newLockId)
{
    scDataNode sqlParams(ict_parent);
    sqlParams.addElement("message_id", scDataNode(msgId));
    sqlParams.addElement("status", scDataNode(status));
    sqlParams.addElement("lock_id", scDataNode(newLockId));
    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update {table_name} "
        "set status = {status}, lock_id = {lock_id}, updated_dt = {updated_dt} "
        "where message_id = {message_id}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);
}

bool grdPersQueueDataModule::setMsgStatusAndUnlock(scString &queueName, ulong64 msgId, uint status, ulong64 lockId)
{
    scDataNode sqlParams(ict_parent);
    sqlParams.addElement("message_id", scDataNode(msgId));
    sqlParams.addElement("status", scDataNode(status));
    sqlParams.addElement("new_lock_id", scDataNode(0));
    sqlParams.addElement("lock_id", scDataNode(lockId));
    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update {table_name} "
        "set status = {status}, lock_id = {new_lock_id}, updated_dt = {updated_dt} "
        "where message_id = {message_id} and lock_id = {lock_id}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

scDateTime grdPersQueueDataModule::getCurrentDt()
{
  scString str = getCurrentDtAsIsoText();
  scDateTime dtInDb = isoStrToDateTime(str);
  return dtInDb;
}

scString grdPersQueueDataModule::getCurrentDtAsIsoText()
{
  scDataNode defValue("", 0);
  scDataNode resNode;
  
  m_db->getValue("select datetime('now')", SC_NULL, resNode, &defValue);
  return resNode.getAsString();
}

bool grdPersQueueDataModule::setItemExecResult(scString &queueName, ulong64 itemId, bool incError, int replyStatus, const scDataNode &result, const scDataNode &error)
{
    scString errorTxt, resultTxt;

    dnSerializer serializer;

    serializer.convToString(result, resultTxt);
    serializer.convToString(error, errorTxt);

    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("message_id", scDataNode(itemId));
    sqlParams.addElement("exec_status", scDataNode(replyStatus));
    sqlParams.addElement("error", scDataNode(errorTxt));
    sqlParams.addElement("result", scDataNode(resultTxt));

    if (incError)
      sqlParams.addElement("inc_error", scDataNode(1));
    else
      sqlParams.addElement("inc_error", scDataNode(0));

    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update {table_name} "
        "set exec_status = {exec_status}, updated_dt = {updated_dt}, "
        "error = {error}, result = {result}, error_cnt = error_cnt + {inc_error} "
        "where message_id = {message_id}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}


class grdPersQueueMsgLoader {
public:
    grdPersQueueMsgLoader(): m_serializer() {}
    virtual ~grdPersQueueMsgLoader() {}
    // run
    void processLoadedRow(scDataNode &row) {
        readStructuredField2(row, "params", scDataNode(), m_serializer, row);
        readStructuredField2(row, "error", scDataNode(), m_serializer, row);
        readStructuredField2(row, "result", scDataNode(), m_serializer, row);

        prepareDateTimeField(row, "added_dt");
        prepareDateTimeField(row, "updated_dt");
    }
protected:
    void readStructuredField(const scDataNode &row, const scString &fieldName, const scDataNode &defValue, dnSerializer &serializer, scDataNode &output)
    {
        scString valueTxt = row.getString(fieldName, "");
        output = defValue;
        if (!valueTxt.empty())
            serializer.convFromString(valueTxt, output);
    }

    void readStructuredField2(const scDataNode &row, const scString &fieldName, const scDataNode &defValue, dnSerializer &serializer, scDataNode &nrow)
    {
        scDataNode value;
        readStructuredField(row, fieldName, defValue, serializer, value);
        nrow.setElementSafe(fieldName, value);
    }

    void prepareDateTimeField(scDataNode &row, const scString &fieldName)
    {
        if (row.getElementType(fieldName) != vt_datetime) 
        {
            scString dbValue = row.getString(fieldName);
            if (dbValue.empty()) {
                row.setElement(fieldName, scDataNode());
            } else {
            //scString isoValue = dbValue.substr(0,4)+dbValue.substr(5,2)+dbValue.substr(8);
                row.setDateTime(fieldName, isoStrToDateTime(dbValue));
            }
        }
    }
protected:
    dnSerializer m_serializer;
};

void grdPersQueueDataModule::loadMessages(const scString &queueName, scDataNode &msgList)
{
    scDataNode sqlParams(ict_parent);
    sqlParams.addElement("status_purge", scDataNode(static_cast<uint>(pqsForPurge)));

    scString sqlText = "select * from  {table_name} "
        "where status <> {status_purge}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    scDbBase::cursor_transporter cur = m_db->select(sqlTextNew, &sqlParams);
    scDataNode row;

    msgList = scDataNode(ict_parent);
    std::auto_ptr<scDataNode> newRow;
    scString paramsTxt, errorTxt, resultTxt;
    scDataNode params, error, result;

    grdPersQueueMsgLoader loader;

    while (!cur->eof())
     {
        row = cur->fetch();

        //newRow.reset(new scDataNode(toString(row.getUInt64("message_id")), row));
        newRow.reset(new scDataNode(row));
    
        loader.processLoadedRow(*newRow);

        msgList.addChild(newRow.release());
     }  
}

ulong64 grdPersQueueDataModule::loadNextLockId(const scString &queueName, ulong64 defValue)
{
    scString sql =
        "select next_lock_id from queue where name = {queue}";

    scDataNode params(ict_parent);
    params.addElement("queue", scDataNode(queueName));

    scDataNode valueNode;

    bool found = m_db->getValue(sql, &params, valueNode);

    ulong64 res;

    if (found && !valueNode.isNull())
        res = valueNode.getAsUInt();
    else 
        res = defValue;

    return res;
}

bool grdPersQueueDataModule::saveNextLockId(const scString &queueName, ulong64 value)
{
    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("name", scDataNode(queueName));
    sqlParams.addElement("next_lock_id", scDataNode(value));

    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update queue "
        "set next_lock_id = {next_lock_id}, updated_dt = {updated_dt} "
        "where name = {name}";

    m_db->execute(sqlText, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

bool grdPersQueueDataModule::lockForPurge(const scString &queueName, ulong64 lockId)
{
    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("status_purge", scDataNode(static_cast<uint>(pqsForPurge)));
    sqlParams.addElement("lock_id", scDataNode(lockId));
    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update {table_name} "
        "set lock_id = {lock_id}, updated_dt = {updated_dt} "
        "where status = {status_purge}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

bool grdPersQueueDataModule::deleteForPurge(const scString &queueName)
{
    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("status_purge", scDataNode(static_cast<uint>(pqsForPurge)));

    scString sqlText = "delete from {table_name} "
        "where status = {status_purge}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

void grdPersQueueDataModule::selectLockedForPurge(const scString &queueName, ulong64 lockId, scDataNode &output)
{
    scDataNode sqlParams(ict_parent);
    sqlParams.addElement("status_purge", scDataNode(static_cast<uint>(pqsForPurge)));
    sqlParams.addElement("lock_id", scDataNode(lockId));

    scString sqlText = "select * from  {table_name} "
        "where status = {status_purge} and lock_id = {lock_id}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    scDbBase::cursor_transporter cur = m_db->select(sqlTextNew, &sqlParams);
    scDataNode row;

    output = scDataNode(ict_list);
    std::auto_ptr<scDataNode> newRow;
    scString paramsTxt, errorTxt, resultTxt;
    scDataNode params, error, result;

    grdPersQueueMsgLoader loader;

    while (!cur->eof())
     {
        row = cur->fetch();
        newRow.reset(new scDataNode(row));

        loader.processLoadedRow(*newRow);

        output.addChild(newRow.release());
     }  
}

bool grdPersQueueDataModule::deleteLockedForPurge(const scString &queueName, ulong64 lockId)
{
    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("status_purge", scDataNode(static_cast<uint>(pqsForPurge)));
    sqlParams.addElement("lock_id", scDataNode(lockId));

    scString sqlText = "delete from {table_name} "
        "where status = {status_purge} and lock_id = {lock_id}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

bool grdPersQueueDataModule::lockMessageList(const scString &queueName, const scDataNode &msgList)
{
    bool res = false;
    ulong64 msgId;

    for(uint i=0, epos = msgList.size(); i != epos; i++) {
        msgId = msgList[i].getUInt64("message_id");
        if (lockMessage(queueName, msgId))
            res = true;
    }

    return res;
}

bool grdPersQueueDataModule::lockMessage(const scString &queueName, ulong64 msgId)
{
    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("status_locked", scDataNode(static_cast<uint>(pqsLocked)));
    sqlParams.addElement("message_id", scDataNode(msgId));
    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update {table_name} "
        "set old_status = status, status = {status_locked}, updated_dt = {updated_dt} "
        "where status <> {status_locked} and message_id = {message_id} and lock_id = 0";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

bool grdPersQueueDataModule::unlockMessageList(const scString &queueName, const scDataNode &msgList)
{
    bool res = false;
    ulong64 msgId;

    for(uint i=0, epos = msgList.size(); i != epos; i++) {
        msgId = msgList[i].getUInt64("message_id");
        if (unlockMessage(queueName, msgId))
            res = true;
    }

    return res;
}

bool grdPersQueueDataModule::unlockMessage(const scString &queueName, ulong64 msgId)
{
    scDataNode sqlParams(ict_parent);

    sqlParams.addElement("status_locked", scDataNode(static_cast<uint>(pqsLocked)));
    sqlParams.addElement("message_id", scDataNode(msgId));
    sqlParams.addChild(scDbBase::newExpr("updated_dt", "datetime('now')"));

    scString sqlText = "update {table_name} "
        "set status = old_status, updated_dt = {updated_dt} "
        "where status = {status_locked} and message_id = {message_id}";

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    m_db->execute(sqlTextNew, &sqlParams);

    return (m_db->rowsAffected() > 0);
}

void grdPersQueueDataModule::getRowsForExport(const scString &queueName, grdPersQueueMsgStatus status, ulong64 msgId, scDataNode &output)
{
    scDataNode sqlParams(ict_parent);
    sqlParams.addElement("status", scDataNode(static_cast<uint>(status)));
    sqlParams.addElement("message_id", scDataNode(msgId));

    scString sqlText = "select * from  {table_name} ";
    scString whereTxt;

    if ((status != pqsUndef) || (msgId > 0)) {
        if (status != pqsUndef)
            whereTxt = "(status = {status})";
        if (msgId > 0) {
            if (!whereTxt.empty())
                whereTxt += " and ";
            whereTxt += "(message_id = {message_id})";
        }
        sqlText += "where "+whereTxt;
    }

    scString tabName = getQueueMsgTableName(queueName);
    scDataNode tplParams(ict_parent);
    tplParams.addElement("table_name", scDataNode(tabName));

    scString sqlTextNew = fillTemplateValues(sqlText, tplParams);

    scDbBase::cursor_transporter cur = m_db->select(sqlTextNew, &sqlParams);
    scDataNode row;

    output = scDataNode(ict_list);
    std::auto_ptr<scDataNode> newRow;
    scString paramsTxt, errorTxt, resultTxt;
    scDataNode params, error, result;

    grdPersQueueMsgLoader loader;

    while (!cur->eof())
     {
        row = cur->fetch();
        newRow.reset(new scDataNode(row));

        loader.processLoadedRow(*newRow);

        output.addChild(newRow.release());
     }  
}

bool grdPersQueueDataModule::loadQueueConfig(const scString &baseName, scDataNode &output)
{
    ulong64 queueId = this->initQueue(baseName, true);

    scDataNode sqlParams(ict_parent);
    sqlParams.addElement("queue_id", scDataNode(queueId));

    scString sqlText = "select * from queue_config where queue_id = {queue_id} ";

    scDbBase::cursor_transporter cur = m_db->select(sqlText, &sqlParams);
    scDataNode row;

    output = scDataNode(ict_parent);
    std::auto_ptr<scDataNode> newRow;

    while (!cur->eof())
     {
        row = cur->fetch();
        newRow.reset(new scDataNode(row.getString("param_value")));
        output.addChild(row.getString("param_name"), newRow.release());
     }  

    return !output.empty();
}

// ----------------------------------------------------------------------------
// grdPersQueueModule
// ----------------------------------------------------------------------------
grdPersQueueModule::grdPersQueueModule()
{
}

grdPersQueueModule::~grdPersQueueModule()
{
    clearManagerLinks();
}

void grdPersQueueModule::init()
{
}


scTaskIntf *grdPersQueueModule::prepareTaskForMessage(scMessage *message)
{
  scTaskIntf *res = SC_NULL;
  scString coreCmd = message->getCoreCommand();
  
  if (
     (message->getInterface() == "pqueue")
     )
  {   
  //init, listen, close, clear, get_status
    if (coreCmd == "open")
    {
      res = prepareQueueManager(message);
    }  
  }  
  
  return res;
}

scTask *grdPersQueueModule::prepareQueueManager(scMessage *message)
{
  std::auto_ptr<grdPersQueueTask> res;

  scDataNode params = message->getParams(); 
  
  if (!params.empty()) {
    scString qname = params.getString("name", "");
    scString baseName = params.getString("base", "");
    if (!baseName.empty()) {
        scDataNode baseParams;
        if (checkDataModule()->loadQueueConfig(baseName, baseParams)) {
            baseParams.merge(params);
            params = baseParams;
        }
    }
    scString execAddr = params.getString("exec_addr", "");
    scString replyToAddr = params.getString("reply_to_addr", "");
    scString errorAddr = params.getString("error_addr", "");
    uint execLimit = params.getUInt("exec_limit", 1);
    uint errorLimit = params.getUInt("error_limit", 99);
    uint errorDelay = params.getUInt("error_delay", PQ_DEF_ERROR_DELAY);
    uint purgeInterval = params.getUInt("purge_interval", PQ_DEF_PURGE_INTERVAL);
    uint storageTimeout = params.getUInt("storage_timeout", PQ_DEF_STORAGE_TIMEOUT);
    uint handleTimeout = params.getUInt("handle_timeout", PQ_DEF_HANDLE_TIMEOUT);
    uint replyTimeout = params.getUInt("reply_timeout", PQ_DEF_REPLY_TIMEOUT);
    scString archiveFName = params.getString("archive_fname", "");
    scString replyCmd = params.getString("reply_cmd", "");

    if (!qname.empty()) {
      res.reset(dynamic_cast<grdPersQueueTask *>(newQueueManagerTask(qname)));
      res->setup(execAddr, replyToAddr, errorAddr, execLimit, errorLimit, errorDelay, purgeInterval, storageTimeout, handleTimeout, replyTimeout, 
          archiveFName, replyCmd);
      m_managers.push_back(res.get());
    }
  }  
  
  return res.release();
}

scTask *grdPersQueueModule::newQueueManagerTask(const scString &queueName)
{
    return new grdPersQueueTask(this, queueName);
}

scStringList grdPersQueueModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("pqueue");  
  return res;
}

scDbBase *grdPersQueueModule::getDatabase()
{
    return m_db.get();
}

void grdPersQueueModule::setDbPath(const scString &path)
{
    m_dbPath = path;
}

scString grdPersQueueModule::getDbPath()
{
    return m_dbPath;
}

void grdPersQueueModule::initDatabase()
{
  m_db.reset(new scDbSqlite());
  m_db->connect(m_dbPath);
  m_dataModule.reset(newDataModule());
  m_dataModule->initSchema();
  scLog::addDebug(scString("pqueue database path: ")+m_dbPath);
}

grdPersQueueDataModule *grdPersQueueModule::checkDataModule()
{
    if (m_dataModule.get() == SC_NULL)
        throw scError("Data module not ready!");
    return m_dataModule.get();
}

grdPersQueueDataModule *grdPersQueueModule::newDataModule()
{
    assert(getDatabase() != SC_NULL);
    return new grdPersQueueDataModule(getDatabase());
}


int grdPersQueueModule::handleMessage(scMessage *message, scResponse &response)
{
 
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "pqueue") 
  {
    if (!isQueueModuleMessage(message->getCoreCommand())) {
      res = forwardQueueMsg(message, response);
    } else if (message->getCoreCommand() == "init") {
      res = handleCmdInit(message, response);
    } else if (message->getCoreCommand() == "open") {
      res = handleCmdOpen(message, response);
    } else if (message->getCoreCommand() == "close") {
      res = handleCmdClose(message, response);
    } else if (message->getCoreCommand() == "drop") {
      res = handleCmdDrop(message, response);
    } else if (message->getCoreCommand() == "qlist") {
      res = handleCmdQList(message, response);
    } else if (message->getCoreCommand() == "define") {
      res = handleCmdDefine(message, response);
    } else if (message->getCoreCommand() == "undefine") {
      res = handleCmdUndefine(message, response);
    }
  }

  return res;
}

bool grdPersQueueModule::isQueueModuleMessage(const scString &coreCmd)
{
    const char* byIdCmds = "|init|open|close|drop|qlist|define|undefine|";
    scString wrk(byIdCmds);
    if (wrk.find(scString("|")+coreCmd+"|") != scString::npos)
        return true;
    else
        return false;
}

int grdPersQueueModule::forwardQueueMsg(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;

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
        grdPersQueueTask *manager = dynamic_cast<grdPersQueueTask *>(checkQueueTask(qname));
        res = manager->handleQueueMsg(message, response);
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

bool grdPersQueueModule::queueExists(const scString &name)
{
  return (findQueue(name) != SC_NULL);
}

scTask *grdPersQueueModule::checkQueueTask(const scString &name)
{
    scTask *res = findQueue(name);
    if (res == SC_NULL)
        throw scError(scString("Unknown queue, name=[")+name+"]");
    return res;
}

scTask *grdPersQueueModule::findQueue(const scString &name)
{
  grdPersQueueTask *res = SC_NULL;
  scString pname;
  
  grdPersQueueTaskPtrList::const_iterator p;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    pname = dynamic_cast<grdPersQueueTask *>(*p)->getQueueName();
    if (pname == name)
    {
      res = dynamic_cast<grdPersQueueTask *>(*p);
      break;
    }  
  }  
  return res;       
}

void grdPersQueueModule::clearTaskRef(scTask *ref)
{
  grdPersQueueTaskPtrList::iterator p;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    if (*p == ref) {
        m_managers.erase(p);
        break;
    }
  }  
}

void grdPersQueueModule::clearManagerLinks()
{
  grdPersQueueTask *task = SC_NULL;
  scString pname;
  
  grdPersQueueTaskPtrList::iterator p;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    task = dynamic_cast<grdPersQueueTask *>(*p);
    task->clearParent();
  }  
}

int grdPersQueueModule::handleCmdInit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString text;

  scDataNode &params = message->getParams(); 

  if (!params.empty())
  {
    scString dbPath = params.getString("dbpath");
    res = SC_MSG_STATUS_OK;                
    setDbPath(dbPath);
    initDatabase();
  }     
  return res;
}

int grdPersQueueModule::handleCmdOpen(scMessage *message, scResponse &response)
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

int grdPersQueueModule::handleCmdClose(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getString(0);
    if (!qname.empty()) {
      grdPersQueueTask *task = dynamic_cast<grdPersQueueTask *>(findQueue(qname));
      if (task == SC_NULL) 
      {
   	res = SC_MSG_STATUS_WRONG_PARAMS;
        scDataNode error;
        error.setElementSafe("text", "Queue not running: ["+qname+"]");
        response.setError(error);   	    
      } else { 
        task->requestStop();
   	res = SC_MSG_STATUS_OK;        
      }      
    } // qname filled    
  } // has children 
           
  return res;
}

int grdPersQueueModule::handleCmdDrop(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    scString qname;
    
    qname = params.getString(0);
    if (!qname.empty()) {
      grdPersQueueTask *task = dynamic_cast<grdPersQueueTask *>(findQueue(qname));
      if (task != SC_NULL) {
        task->requestStop();
        res = SC_MSG_STATUS_WRONG_CFG;
      } else {
        checkDataModule()->dropQueue(qname);
        res = SC_MSG_STATUS_OK;        
      }
    } // qname filled    
  } // has children 
           
  return res;
}

int grdPersQueueModule::handleCmdQList(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  scDataNode result(ict_list);
  getQueueList(result);
  response.setResult(result);
  res = SC_MSG_STATUS_OK;        
           
  return res;
}

int grdPersQueueModule::handleCmdDefine(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (params.hasChild("queue") && params.hasChild("config")) {
      scString queueName = params.getString("queue");
      scDataNode &configParams = params["config"];
      checkDataModule()->defineQueue(queueName, configParams);
      res = SC_MSG_STATUS_OK;        
  }
          
  return res;
}

int grdPersQueueModule::handleCmdUndefine(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (params.hasChild("queue")) {
      scString queueName = params.getString("queue");
      checkDataModule()->undefineQueue(queueName);
      res = SC_MSG_STATUS_OK;        
  }
          
  return res;
}

void grdPersQueueModule::getQueueList(scDataNode &output)
{
  output = scDataNode(ict_list);

  grdPersQueueTask *task = SC_NULL;
  
  grdPersQueueTaskPtrList::const_iterator p;
  std::auto_ptr<scDataNode> newRow;

  for (p = m_managers.begin(); p != m_managers.end(); ++p) {
    task = dynamic_cast<grdPersQueueTask *>(*p);
    newRow.reset(new scDataNode(ict_parent));
    newRow->setElementSafe("name", scDataNode(task->getQueueName()));
    newRow->setElementSafe("msg_cnt", scDataNode(task->getMessageCount()));
    output.addChild(newRow.release());
  }  
}

// ----------------------------------------------------------------------------
// grdPersQueueTask
// ----------------------------------------------------------------------------
grdPersQueueTask::grdPersQueueTask(grdPersQueueModule *parent, const scString &queueName): m_parent(parent), m_queueName(queueName), scTask()
{
  setName(scString(PQ_QMAN_TASK_NAME_PFX)+queueName);
  m_nextLockId = 1;
  m_lastStatusCheck = cpu_time_ms();
  m_statusCheckDelay = PQ_DEF_STATUS_CHK_DELAY;
  m_lastPurge = cpu_time_ms();
  m_purgeInterval = PQ_DEF_PURGE_INTERVAL;
}

grdPersQueueTask::~grdPersQueueTask()
{
    if (m_parent != SC_NULL)
        m_parent->clearTaskRef(this);
}

void grdPersQueueTask::clearParent()
{
    m_parent = SC_NULL;
}

scString grdPersQueueTask::getQueueName()
{
    return m_queueName;
}

ulong64 grdPersQueueTask::getQueueId()
{
    return m_queueId;
}

ulong64 grdPersQueueTask::genNewLockId()
{
    ulong64 res = m_nextLockId;
    m_nextLockId++;
    if ((res % PQ_LOCK_SAVE_FREQ) == 1) // first item in block
        checkDataModule()->saveNextLockId(getQueueName(), res + PQ_LOCK_SAVE_FREQ);
    return res;
}

void grdPersQueueTask::initNextLockId()
{
   m_nextLockId = checkDataModule()->loadNextLockId(getQueueName(), 1);
}

bool grdPersQueueTask::needsRun()
{
  return isTimeForStatusCheck() || isTimeForPurge();
}

int grdPersQueueTask::intRun()
{
  int res = scTask::intRun();
  checkScanStatusesNeeded();
  checkPurgeNeeded();
  res = res + 1;
  return res;
}

void grdPersQueueTask::setup(const scString &execAddr, const scString &replyToAddr, const scString &errorAddr, 
    uint execLimit, uint errorLimit, uint errorDelay, uint purgeInterval, 
    uint storageTimeout, uint handleTimeout, uint replyTimeout, const scString &archiveFName, const scString &replyCmd)
{
    m_execAddr = execAddr;
    m_replyToAddr = replyToAddr;
    m_errorAddr = errorAddr;
    m_execLimit = execLimit;
    m_errorLimit = errorLimit;
    m_errorDelay = errorDelay;
    m_purgeInterval = purgeInterval;
    m_storageTimeout = storageTimeout;
    m_handleTimeout = handleTimeout;
    m_replyTimeout = replyTimeout;
    m_archiveFName = archiveFName;
    m_replyCmd = replyCmd;

    m_queueId = checkDataModule()->initQueue(m_queueName, false);
}

void grdPersQueueTask::intInit()
{
    initNextLockId();

    performPurge();

    loadMessages();
    restartMessages();
    scanForExecute();
}

grdPersQueueDataModule *grdPersQueueTask::checkDataModule()
{
    if (m_parent == SC_NULL)
      throw scError("Parent not available");

    return m_parent->checkDataModule();
}

void grdPersQueueTask::loadMessages()
{
    scDataNode msgList;
    checkDataModule()->loadMessages(getQueueName(), msgList);
    m_msgList.setItemList(msgList);
}


void grdPersQueueTask::scanForExecute()
{
    for(uint i=0, epos = m_msgList.size(); i != epos; i++)
    {
       if (m_msgList.getItemStatusAt(i) == pqsReady) {
           if (!canExecuteAnyMsg()) 
              break;
           else
              checkExecuteMsg(m_msgList.getItemIdAt(i));
       }
    }
}

// revert message statuses, when applicable, clear lock_id:
// - pqsSent -> pqsReady
// - pqsExecError -> pqsReady
// - pqsReplySent -> pqsHandled
// - pqsReplyError -> pqsHandled
void grdPersQueueTask::restartMessages()
{
    scDateTime nowDt = checkDataModule()->getCurrentDt();
    ulong64 msgId;
    uint errorCnt;
    scDataNode itemRec;
    std::set<ulong64> handledSet;

    for(uint i=0, epos = m_msgList.size(); i != epos; i++)
    {
        itemRec = m_msgList.getItemAt(i); 

        if (itemRec["error_cnt"].isNull()) {
            errorCnt = 0;
        } else {
            errorCnt = itemRec.getUInt("error_cnt", 0);
        }

        switch (m_msgList.getItemStatusAt(i)) {
        case pqsExecError:
            if ((m_errorLimit > 0) && (errorCnt >= m_errorLimit))
                break;
        case pqsSent:
            msgId = m_msgList.getItemIdAt(i);
            changeMsgStatusAndLock(msgId, pqsReady, 0, nowDt);
            break;
        case pqsReplyError:
            if ((m_errorLimit > 0) && (errorCnt >= m_errorLimit))
                break;
        case pqsReplySent:
            msgId = m_msgList.getItemIdAt(i);
            changeMsgStatusAndLock(msgId, pqsHandled, 0, nowDt);
            break;
        case pqsHandled:
            msgId = m_msgList.getItemIdAt(i);
            handledSet.insert(msgId);
            break;
        default:
            ; // do nothing
        }
    }

    // we need to perform this by msg-id, not index because each processing can remove message
    for(std::set<ulong64>::const_iterator it = handledSet.begin(), epos = handledSet.end(); it != epos; ++it)
    {
        processHandledMsg(*it);
    }
}

int grdPersQueueTask::handleQueueMsg(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);

  if (message->getInterface() == "pqueue") 
  {
    if (message->getCoreCommand() == "put") {
      res = handleCmdPut(message, response);
    } else if (message->getCoreCommand() == "mlist") {
      res = handleCmdMList(message, response);
    } else if (message->getCoreCommand() == "fetch") {
      res = handleCmdFetch(message, response);
    } else if (message->getCoreCommand() == "handled") {
      res = handleCmdHandled(message, response);
    } else if (message->getCoreCommand() == "lock") {
      res = handleCmdLock(message, response);
    } else if (message->getCoreCommand() == "unlock") {
      res = handleCmdUnlock(message, response);
    } else if (message->getCoreCommand() == "cancel") {
      res = handleCmdCancel(message, response);
    } else if (message->getCoreCommand() == "peek") {
      res = handleCmdPeek(message, response);
    } else if (message->getCoreCommand() == "export") {
      res = handleCmdExport(message, response);
    } else if (message->getCoreCommand() == "import") {
      res = handleCmdImport(message, response);
    } else if (message->getCoreCommand() == "register") {
      res = handleCmdRegister(message, response);
    } else if (message->getCoreCommand() == "purge") {
      res = handleCmdPurge(message, response);
    }
  }
  return res;
}

int grdPersQueueTask::handleCmdPut(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("msg_list")) {
      msgList = params["msg_list"];
  } else if (params.hasChild("msg_command")) {
      std::auto_ptr<scDataNode> msg(new scDataNode(ict_parent));
      msg->addElement("msg_command", params["msg_command"]);
      if (params.hasChild("msg_params"))
        msg->addElement("msg_params", params["msg_params"]);
      if (params.hasChild("msg_ref"))
        msg->addElement("msg_ref", params["msg_ref"]);
      msgList.addChild(msg.release());
  }

  if (msgList.empty() && !msgList.isNull()) {
    dnSerializer serializer;
    scString listText = msgList.getAsString();
    serializer.convFromString(listText, msgList);
  }

  if (!msgList.empty()) {
      addMessageList(msgList);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdPersQueueTask::handleCmdMList(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  scDataNode result;
  getMessageList(result);
  response.setResult(result);
  res = SC_MSG_STATUS_OK;        
           
  return res;
}

int grdPersQueueTask::handleCmdFetch(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  uint msgLimit = 1;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  msgLimit = params.getUInt("limit", msgLimit);

  scDataNode result;
  fetchMessageList(msgLimit, result);
  response.setResult(result);
  res = SC_MSG_STATUS_OK;        
           
  return res;
}

int grdPersQueueTask::handleCmdHandled(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  uint msgLimit = 1;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (params.hasChild("lock_id")) {
      ulong64 lockId = params.getUInt("lock_id");
      ulong64 msgId;
      int globalReplyStatus = params.getInt("exec_status");
      scDataNode globalResult, globalError;
      
      if (params.hasChild("result"))
        params.getElement("result", globalResult);

      if (params.hasChild("error"))
        params.getElement("error", globalError);

      if (params.hasChild("message_id")) {
          if (performMsgHandled(params.getUInt64("message_id"), lockId, globalReplyStatus, globalResult, globalError)) {
              res = SC_MSG_STATUS_OK;        
          }
      } else if (params.hasChild("msg_list")) {
      // msg list
          scDataNode &msgList = params["msg_list"];
          int itemReplyStatus;
          scDataNode itemResult, itemError;

          for(uint i=0, epos = msgList.size(); i != epos; i++) {
              itemReplyStatus = msgList[i].getInt("exec_status", globalReplyStatus);

              if (msgList[i].hasChild("result"))
                msgList[i].getElement("result", itemResult);
              else
                itemResult = globalResult;

              if (msgList[i].hasChild("error"))
                msgList[i].getElement("error", itemError);
              else
                itemError = globalError;

              msgId = msgList[i].getUInt64("message_id");
              if (performMsgHandled(msgId, lockId, itemReplyStatus, itemResult, itemError)) {
                  res = SC_MSG_STATUS_OK;        
              } // perform OK
          } // for
      } // if msg list
  } // if lock-id specified
           
  return res;
}

int grdPersQueueTask::handleCmdLock(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("msg_list")) {
      msgList = params["msg_list"];
  } else if (params.hasChild("message_id")) {
      std::auto_ptr<scDataNode> msg(new scDataNode(ict_parent));
      msg->addElement("message_id", params["message_id"]);
      msgList.addChild(msg.release());
  }

  if (msgList.empty() && !msgList.isNull()) {
    dnSerializer serializer;
    scString listText = msgList.getAsString();
    serializer.convFromString(listText, msgList);
  }

  if (!msgList.empty()) {
      lockMessageList(msgList);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdPersQueueTask::handleCmdUnlock(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("msg_list")) {
      msgList = params["msg_list"];
  } else if (params.hasChild("message_id")) {
      std::auto_ptr<scDataNode> msg(new scDataNode(ict_parent));
      msg->addElement("message_id", params["message_id"]);
      msgList.addChild(msg.release());
  }

  if (msgList.empty() && !msgList.isNull()) {
    dnSerializer serializer;
    scString listText = msgList.getAsString();
    serializer.convFromString(listText, msgList);
  }

  if (!msgList.empty()) {
      unlockMessageList(msgList);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdPersQueueTask::handleCmdCancel(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 

  scDataNode msgList(ict_list);

  if (params.hasChild("msg_list")) {
      msgList = params["msg_list"];
  } else if (params.hasChild("message_id")) {
      std::auto_ptr<scDataNode> msg(new scDataNode(ict_parent));
      msg->addElement("message_id", params["message_id"]);
      msgList.addChild(msg.release());
  }

  if (msgList.empty() && !msgList.isNull()) {
    dnSerializer serializer;
    scString listText = msgList.getAsString();
    serializer.convFromString(listText, msgList);
  }

  if (!msgList.empty()) {
      cancelMessageList(msgList);
      res = SC_MSG_STATUS_OK;
  }

  return res;
}

int grdPersQueueTask::handleCmdPeek(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  uint msgLimit = 1;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (params.hasChild("message_id")) {
      ulong64 msgId = params.getUInt64("message_id");

      scDataNode result;
      if (peekMessage(msgId, result)) {
        response.setResult(result);
        res = SC_MSG_STATUS_OK;        
      }
  } else if (params.hasChild("reference")) {
      scString ref = params.getString("reference");

      scDataNode result;
      if (peekMessage(ref, result)) {
        response.setResult(result);
        res = SC_MSG_STATUS_OK;        
      }
  }

           
  return res;
}

int grdPersQueueTask::handleCmdExport(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  uint msgLimit = 1;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (params.hasChild("fpath")) {
      scString fpath = params.getString("fpath");
      grdPersQueueMsgStatus status = static_cast<grdPersQueueMsgStatus>(params.getUInt("status", pqsUndef));
      ulong64 msgId = params.getUInt64("message_id", 0);

      exportMessages(fpath, status, msgId);
      res = SC_MSG_STATUS_OK;        
  }
           
  return res;
}

int grdPersQueueTask::handleCmdImport(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  uint msgLimit = 1;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (params.hasChild("fpath")) {
      scString fpath = params.getString("fpath");
      importMessages(fpath);
      res = SC_MSG_STATUS_OK;        
  }
           
  return res;
}

int grdPersQueueTask::handleCmdRegister(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  uint msgLimit = 1;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  scString aliasName = params.getString("alias", "");
  scString execAtAddr = params.getString("exec_at_addr", "");

  registerQueue(aliasName, execAtAddr);
  res = SC_MSG_STATUS_OK;        
           
  return res;
}

int grdPersQueueTask::handleCmdPurge(scMessage *message, scResponse &response)
{
  performPurge();
  return SC_MSG_STATUS_OK;
}

bool grdPersQueueTask::performMsgHandled(ulong64 msgId, ulong64 lockId, int replyStatus, const scDataNode &result, const scDataNode &error)
{
    bool res = false;
    logInfo(msgId, scString("pqueue.handled arrived, status: ") + toString(replyStatus)+", lock="+toString(lockId));

    if (replyStatus == SC_MSG_STATUS_OK) {
        res = setMsgExecSuccess(msgId, lockId, replyStatus, result);
        if (!res) 
          logWarning(msgId, scString("Outdated submit result arrived, lock="+toString(lockId)));
        else
          processHandledMsg(msgId);
    } else {
// error
        res = setMsgExecError(msgId, lockId, error);
        if (!res)
            logWarning(msgId, scString("Outdated submit error: "+error.dump()+", lock: "+toString(lockId)));
    }

    handleExecEnd();
    return res;
}

void grdPersQueueTask::getMessageList(scDataNode &output)
{
  output = scDataNode(ict_list);

  grdPersQueueTask *task = SC_NULL;
  scDataNode msgRec;
  std::auto_ptr<scDataNode> newMsgRec;
  
  for (uint i=0, epos = m_msgList.size(); i != epos; i++) {
    m_msgList.getItemAt(i, msgRec);
    newMsgRec.reset(new scDataNode(ict_parent));
    newMsgRec->setElementSafe("message_id", scDataNode(msgRec.getUInt64("message_id")));
    newMsgRec->setElementSafe("status", scDataNode(msgRec.getUInt("status")));
    newMsgRec->setElementSafe("command", scDataNode(msgRec.getString("command")));
    newMsgRec->setElementSafe("added_dt", scDataNode(dateTimeToIsoStr(msgRec.getDateTime("added_dt"))));
    if (msgRec.hasChild("updated_dt") && (msgRec.getElementType("updated_dt") != vt_null)) {
        newMsgRec->setElementSafe("updated_dt", scDataNode(dateTimeToIsoStr(msgRec.getDateTime("updated_dt"))));
    } else {
        newMsgRec->setElementSafe("updated_dt", scDataNode(""));
    }
    newMsgRec->setElementSafe("size", 
        scDataNode(m_msgList.getItemSizeAt(i))
    );
    output.addChild(newMsgRec.release());
  }  
}

void grdPersQueueTask::fetchMessageList(uint limit, scDataNode &output)
{
    std::auto_ptr<scDataNode> newMsg;
    bool bFound;

    std::auto_ptr<scDataNode> msgList(new scDataNode(ict_list));
    output = scDataNode(ict_parent);

    ulong64 lockId = 0;

    do {
        newMsg.reset(new scDataNode());
        bFound = fetchMessage(*newMsg, lockId);
        if (bFound) {
            msgList->addChild(newMsg.release());
        }
    } while((msgList->size() < limit) && bFound);

    if (!msgList->empty())
      output.setElementSafe("lock_id", scDataNode(lockId));
    output.addChild("msg_list", msgList.release());
}

bool grdPersQueueTask::fetchMessage(scDataNode &output, ulong64 &lockId)
{
    ulong64 msgId;
    bool res = m_msgList.findReadyMessage(msgId);
    if (res) {
        if (lockId == 0) {
            lockId = genNewLockId();
        }
        changeMsgStatusAndLock(msgId, pqsSent, lockId);
        std::auto_ptr<scDataNode> newMsgRec(new scDataNode(ict_parent));
        newMsgRec->setElementSafe("message_id", scDataNode(msgId));
        scDataNode msgDets;
        
        if (!m_msgList.getItem(msgId, msgDets))
            throw scError("Unknown message: "+toString(msgId));

        newMsgRec->setElementSafe("command", scDataNode(msgDets.getString("command")));
        newMsgRec->setElementSafe("params", msgDets.getElement("params"));

        output.addChild(newMsgRec.release());
    }

    return res;
}

int grdPersQueueTask::handleMessage(scEnvelope &envelope, scResponse &response)
{
    assert(!envelope.getEvent()->isResponse());
    ulong64 msgId = putMessage(dynamic_cast<scMessage *>(envelope.getEvent()));

    scDataNode result(ict_parent);
    result.setElementSafe("reference", scDataNode(toString(msgId)));
    response.setResult(result);

    return SC_MSG_STATUS_OK;
}

ulong64 grdPersQueueTask::putMessage(scMessage *message)
{
    std::auto_ptr<scDataNode> msg(new scDataNode(ict_parent));
    msg->addElement("msg_command", scDataNode(message->getCommand()));
    if (message->hasParams())
        msg->addElement("msg_params", message->getParams());
    msg->addElement("msg_ref", scDataNode(""));

    return addMessage(*msg);
}

void grdPersQueueTask::addMessageList(const scDataNode &msgList)
{
    for(uint i=0, epos = msgList.size(); i != epos; i++)
      addMessage(msgList.getElement(i));
}

ulong64 grdPersQueueTask::addMessage(const scDataNode &msgRec)
{
  uint msgId = insertMsgToDb(msgRec, pqsReady);
  insertMsgToMemQueue(msgId, msgRec, pqsReady);  
  checkExecuteMsg(msgId);
  return msgId;
}

ulong64 grdPersQueueTask::insertMsgToDb(const scDataNode &msgRec, uint status)
{
    scDataNode params, ref;
    if (msgRec.hasChild("msg_params"))
        msgRec.getElement("msg_params", params);
    if (msgRec.hasChild("msg_ref"))
        msgRec.getElement("msg_ref", ref);
    scString replyCmd;
    if (msgRec.hasChild("reply_cmd"))
        replyCmd = msgRec.getString("reply_cmd");
    
    return checkDataModule()->insertMessage(getQueueName(), msgRec.getString("msg_command", ""), params, ref, replyCmd, status);
}

void grdPersQueueTask::insertMsgToMemQueue(ulong64 msgId, const scDataNode &msgRec, uint status)
{
    std::auto_ptr<scDataNode> msgGuard(m_msgList.newMessage(msgId));
    msgGuard->setAsParent();
    msgGuard->setElementSafe("message_id", scDataNode(msgId));
    msgGuard->setElementSafe("command", scDataNode(msgRec.getString("msg_command")));
    if (msgRec.hasChild("msg_params"))
      msgGuard->setElementSafe("params", msgRec.getElement("msg_params"));
    if (msgRec.hasChild("msg_ref"))
      msgGuard->setElementSafe("reference", msgRec.getElement("msg_ref"));
    if (msgRec.hasChild("reply_cmd"))
      msgGuard->setElementSafe("reply_cmd", msgRec.getElement("reply_cmd"));
    msgGuard->setElementSafe("status", scDataNode(status));
    msgGuard->setElementSafe("error_cnt", scDataNode(0));
    msgGuard->setElementSafe("lock_id", scDataNode(0));
    //here we lie a little - added_dt is different in database, will be corrected on message reload (if required)
    msgGuard->setElementSafe("added_dt", scDataNode(checkDataModule()->getCurrentDt()));
    m_msgList.insert(msgGuard.release());
}

grdPersQueueModule *grdPersQueueTask::checkParent() 
{
   if (m_parent == SC_NULL)
       throw scError("Parent not assigned");
   return m_parent;
}

bool grdPersQueueTask::canExecuteAnyMsg() {
  bool res = false;
  if ((m_execLimit == 0) || (m_execLimit > getActiveMsgCount())){
      if (!getExecAddr().empty()) {
          res = true;
      }
  }
  return res;
}

void grdPersQueueTask::checkExecuteMsg(ulong64 msgId)
{
  if (canExecuteAnyMsg()) {
      scString keyName = m_msgList.getKeyName(msgId);
  
      if (m_msgList.hasItem(keyName))
      {  
        uint status = m_msgList.getItemStatus(keyName);
        if (status == pqsReady)
          executeMsg(msgId);
      }  
  }
}

scString grdPersQueueTask::getExecAddr() const
{
    return this->m_execAddr;
}

scString grdPersQueueTask::getReplyToAddr() const
{
    return this->m_replyToAddr;
}

void grdPersQueueTask::executeMsg(ulong64 msgId)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(getExecAddr()));
  std::auto_ptr<scMessage> messageGuard(new scMessage());

  scDataNode msgRec;
  assert(m_msgList.getItem(msgId, msgRec));

  messageGuard->setCommand(msgRec.getString("command"));
  messageGuard->setRequestId(getScheduler()->getNextRequestId());

  if (msgRec.hasChild("params"))
    messageGuard->setParams(msgRec.getElement("params"));

  envelopeGuard->setEvent(messageGuard.release());
  envelopeGuard->setTimeout(m_handleTimeout);

  ulong64 lockId = genNewLockId();

  changeMsgStatusAndLock(msgId, pqsSent, lockId);

  getScheduler()->postEnvelope(envelopeGuard.release(), 
    new grdPersQueueExecHandler(this, msgId, lockId));
}


void grdPersQueueTask::changeMsgStatusAndLock(ulong64 msgId, grdPersQueueMsgStatus status, ulong64 lockId)
{
    changeMsgStatusAndLock(msgId, status, lockId, checkDataModule()->getCurrentDt());
}

void grdPersQueueTask::changeMsgStatusAndLock(ulong64 msgId, grdPersQueueMsgStatus status, ulong64 lockId, scDateTime nowDt)
{
    m_msgList.setStatusAndLock(msgId, status, lockId, nowDt);
    checkDataModule()->setMsgStatusAndLock(getQueueName(), msgId, status, lockId);
}

bool grdPersQueueTask::changeMsgStatusAndUnlock(ulong64 msgId, grdPersQueueMsgStatus status, ulong64 lockId)
{
    bool res = false;
    if (checkDataModule()->setMsgStatusAndUnlock(getQueueName(), msgId, status, lockId)) {
        m_msgList.setStatusAndLock(msgId, status, 0, checkDataModule()->getCurrentDt());
        res = true;
    }
    return res;
}

void grdPersQueueTask::changeMsgStatus(ulong64 msgId, grdPersQueueMsgStatus status)
{
   checkDataModule()->setMsgStatus(getQueueName(), msgId, status);
   m_msgList.setStatus(msgId, status, checkDataModule()->getCurrentDt());
}

// set db status to "exec_error" and unlock
// when success:
// - set memory status to "error" 
// - set "error"
// - set reply status
// - set "updated_dt" time
bool grdPersQueueTask::setMsgExecError(ulong64 msgId, uint lockId, const scString &errorMsg)
{
  bool res = changeMsgStatusAndUnlock(msgId, pqsExecError, lockId);
  if (res) {
      m_msgList.setItemExecResult(msgId, SC_MSG_STATUS_ERROR, scDataNode(""), scDataNode(errorMsg));
      checkDataModule()->setItemExecResult(getQueueName(), msgId, true, SC_MSG_STATUS_ERROR, scDataNode(""), scDataNode(errorMsg));
  }
  return res;
}

bool grdPersQueueTask::setMsgExecError(ulong64 msgId, uint lockId, const scDataNode &error)
{
  bool res = changeMsgStatusAndUnlock(msgId, pqsExecError, lockId);
  if (res) {
      m_msgList.setItemExecResult(msgId, SC_MSG_STATUS_ERROR, scDataNode(""), error);
      checkDataModule()->setItemExecResult(getQueueName(), msgId, true, SC_MSG_STATUS_ERROR, scDataNode(""), error);
  }
  return res;
}

bool grdPersQueueTask::setMsgExecSuccess(ulong64 msgId, uint lockId, uint resultStatus, const scDataNode &result)
{
  bool res = changeMsgStatusAndUnlock(msgId, pqsHandled, lockId);
  if (res) {
      m_msgList.setItemExecResult(msgId, resultStatus, result, scDataNode(""));
      checkDataModule()->setItemExecResult(getQueueName(), msgId, false, resultStatus, result, scDataNode(""));
  }
  return res;
}

void grdPersQueueTask::handleSubmitError(ulong64 msgId, uint lockId, const scString &errorMsg)
{
    if (setMsgExecError(msgId, lockId, errorMsg))
        logError(msgId, scString("Submit error: "+errorMsg+", lock: "+toString(lockId)));
    else
        logError(msgId, scString("Outdated submit error: "+errorMsg+", lock: "+toString(lockId)));
  
    handleExecEnd();
}

void grdPersQueueTask::handleSubmitResultOK(ulong64 msgId, uint lockId, const scMessage &message, const scResponse &a_response)
{
    if (!setMsgExecSuccess(msgId, lockId, a_response.getStatus(), a_response.getResult())) {
        logWarning(msgId, scString("Outdated submit result arrived, lock="+toString(lockId)));
        return;
    }

    logInfo(msgId, scString("Submit ended OK, lock="+toString(lockId)));

    processHandledMsg(msgId);
    handleExecEnd();
}

void grdPersQueueTask::processHandledMsg(ulong64 msgId)
{
  if (!getReplyToAddr().empty())
      performReply(msgId);
  else {
      prepareMsgForPurge(msgId);
  }
}

void grdPersQueueTask::logWarning(ulong64 msgId, const scString &msgText)
{
    scLog::addWarning(msgText+", context: [msg-id="+toString(msgId)+"]");
}

void grdPersQueueTask::logError(ulong64 msgId, const scString &msgText)
{
    scLog::addError(msgText+", context: [msg-id="+toString(msgId)+"]");
}

void grdPersQueueTask::logInfo(ulong64 msgId, const scString &msgText)
{
    scLog::addInfo(msgText+", context: [msg-id="+toString(msgId)+"]");
}

// - change status in memory "reply sent" - to protect agains scanner deamons 
// - send reply message, 
// - on success change status to "for purge"
void grdPersQueueTask::performReply(ulong64 msgId)
{
    m_msgList.setStatus(msgId, pqsReplySent, checkDataModule()->getCurrentDt());

    std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
    envelopeGuard->setReceiver(scMessageAddress(getReplyToAddr()));
    std::auto_ptr<scMessage> messageGuard(new scMessage());

    scDataNode msgRec;
    assert(m_msgList.getItem(msgId, msgRec));

    scString replyCmd = msgRec.getString("reply_cmd", "");
    if (replyCmd.empty())
        replyCmd = m_replyCmd;
    if (replyCmd.empty())
        replyCmd = "pqueue.reply";
    messageGuard->setCommand(replyCmd);
    messageGuard->setRequestId(getScheduler()->getNextRequestId());

    scDataNode params(ict_parent);

    scString refText = msgRec.getString("reference", "");
    if (strTrim(refText).empty()) {
        refText = toString(msgId);
    }
    params.setElementSafe("reference", scDataNode(refText));

    params.setElementSafe("exec_status", scDataNode(msgRec.getInt("exec_status", -1)));
    if (msgRec.hasChild("result"))
      params.setElementSafe("result", msgRec.getElement("result"));
    if (msgRec.hasChild("error"))
      params.setElementSafe("error", msgRec.getElement("error"));

    messageGuard->setParams(params);
    envelopeGuard->setEvent(messageGuard.release());
    envelopeGuard->setTimeout(m_replyTimeout);

    getScheduler()->postEnvelope(envelopeGuard.release(), 
      new grdPersQueueReplyHandler(this, msgId));
}

void grdPersQueueTask::handleReplyError(ulong64 msgId, uint lockId, const scString &errorMsg)
{
    logError(msgId, scString("Reply error: "+errorMsg+", lock: "+toString(lockId)));
    changeMsgStatus(msgId, pqsReplyError);
}

void grdPersQueueTask::handleReplyResultOK(ulong64 msgId, uint lockId, const scMessage &message)
{
    prepareMsgForPurge(msgId);
}

uint grdPersQueueTask::getActiveMsgCount() 
{
    return m_msgList.getItemCountByStatus(pqsSent);
}

bool grdPersQueueTask::isTimeForStatusCheck()
{
  bool res = is_cpu_time_elapsed(m_lastStatusCheck, m_statusCheckDelay);
  return res;    
}

void grdPersQueueTask::checkScanStatusesNeeded()
{
    if (isTimeForStatusCheck()) {
        performStatusCheck();
        m_lastStatusCheck = cpu_time_ms();
    }
}

void grdPersQueueTask::performStatusCheck()
{
    scanForRetryErrors();
    scanForExecTimeout();
    scanForExecute();
    scanForStorageTimeout();
}

void grdPersQueueTask::handleExecEnd()
{
    if (!m_execAddr.empty())
       scanForExecute();
}

void grdPersQueueTask::scanForRetryErrors()
{
    cpu_ticks nowTicks = dateTimeToMSecs(currentDateTime());
    cpu_ticks lowerTicks = nowTicks - m_errorDelay;
    grdPersQueueMsgStatus status;
    ulong64 msgId;
    uint errorCnt;
    
    for(uint i=0, epos = m_msgList.size(); i != epos; i++)
    {
       status = m_msgList.getItemStatusAt(i);
       msgId = m_msgList.getItemIdAt(i);
        
       if ((status == pqsExecError) || (status == pqsReplyError)) {
           errorCnt = m_msgList.getItemAt(i).getUInt("error_cnt", 0);

           if ((m_errorLimit == 0) || (errorCnt < m_errorLimit))
           {
               if (dateTimeToMSecs(m_msgList.getItemUpdatedDtAt(i)) < lowerTicks)
               {
                   if (status == pqsExecError) {
                       changeMsgStatus(msgId, pqsReady);
                   } else if (status == pqsReplyError) {
                       changeMsgStatus(msgId, pqsHandled);
                   }
               } // if it's time 
           } // if error cnt < limit 
       } // if status = error
    } // for
}

void grdPersQueueTask::scanForExecTimeout()
{
    if (!m_execAddr.empty())
        return;

    if (m_handleTimeout == 0)
        return;

    cpu_ticks nowTicks = dateTimeToMSecs(currentDateTime());
    scDateTime currDbTime = checkDataModule()->getCurrentDt();
    cpu_ticks lowerTicks = nowTicks - m_handleTimeout;
    grdPersQueueMsgStatus status;
    ulong64 msgId;
    
    for(uint i=0, epos = m_msgList.size(); i != epos; i++)
    {
       status = m_msgList.getItemStatusAt(i);
       msgId = m_msgList.getItemIdAt(i);
        
       if ((status == pqsSent) && (dateTimeToMSecs(m_msgList.getItemUpdatedDtAt(i) < lowerTicks))) {
           logError(msgId, scString("Handle timeout"));
           changeMsgStatusAndLock(msgId, pqsReady, 0, currDbTime);
       } 
    }
}

void grdPersQueueTask::scanForStorageTimeout()
{
    if (m_storageTimeout == 0)
        return;

    scDateTime currDbTime = checkDataModule()->getCurrentDt();
    scDateTime addedDt;
    cpu_ticks nowTicks = dateTimeToMSecs(currDbTime);
    cpu_ticks lowerTicks = nowTicks - m_storageTimeout;
    grdPersQueueMsgStatus status;
    ulong64 msgId;
    
    std::set<ulong64> delSet;

    for(uint i=0, epos = m_msgList.size(); i != epos; i++)
    {
       status = m_msgList.getItemStatusAt(i);
       msgId = m_msgList.getItemIdAt(i);
       addedDt = m_msgList.getItemAddedDtAt(i);
        
       if ((status == pqsReady) && (dateTimeToMSecs(addedDt) < lowerTicks)) {
           delSet.insert(msgId);
       } 
    }

    // perform loop using set because each step can remove message from list
    for(std::set<ulong64>::const_iterator it = delSet.begin(), epos = delSet.end(); it != epos; ++it)
    {
       logError(*it, scString("Storage timeout"));
       prepareMsgForPurge(*it, currDbTime);
    }
}

void grdPersQueueTask::prepareMsgForPurge(ulong64 msgId)
{
    prepareMsgForPurge(msgId, checkDataModule()->getCurrentDt());
}

void grdPersQueueTask::prepareMsgForPurge(ulong64 msgId, scDateTime chgTime)
{
   changeMsgStatusAndLock(msgId, pqsForPurge, 0, chgTime);
   m_msgList.erase(msgId);
}

void grdPersQueueTask::checkPurgeNeeded()
{
    if (isTimeForPurge())
    {
        performPurge();
    }
}

bool grdPersQueueTask::useArchiveForPurge()
{
    return !m_archiveFName.empty();
}

bool grdPersQueueTask::isTimeForPurge()
{
  return (m_purgeInterval > 0) && is_cpu_time_elapsed(m_lastPurge, m_purgeInterval);    
}

void grdPersQueueTask::performPurge()
{
    if (useArchiveForPurge()) {
        performPurgeWithArchive();
    } else {
        performPurgeDelOnly();
    }
    m_lastPurge = cpu_time_ms();
}

void grdPersQueueTask::performPurgeDelOnly()
{
    if (checkDataModule()->deleteForPurge(getQueueName()))
        scLog::addInfo("Purge performed successfully");
}

bool grdPersQueueTask::lockForPurge(ulong64 lockId)
{
    return checkDataModule()->lockForPurge(getQueueName(), lockId);
}

void grdPersQueueTask::performPurgeWithArchive()
{
    ulong64 lockId = genNewLockId();
    if (lockForPurge(lockId)) {
        scDataNode rows;
        checkDataModule()->selectLockedForPurge(getQueueName(), lockId, rows);
        saveArchive(m_archiveFName, rows);
        checkDataModule()->deleteLockedForPurge(getQueueName(), lockId);
    }
}

void grdPersQueueTask::saveArchive(const scString &fnameTpl, const scDataNode &rows)
{
    scString fname = findFreeFileName(fnameTpl, "*");
    saveArchiveToFile(fname, rows);
}

void grdPersQueueTask::saveArchiveToFile(const scString &fpath, const scDataNode &rows)
{
    scDataNode nrows;
    reformatArchiveRows(rows, nrows);

    dnSerializer serializer;
    scString fileTxt;
    serializer.convToString(nrows, fileTxt);

    char *buffer_ptr = const_cast<char *>(stringToCharPtr(fileTxt));
    void *vPtr = static_cast<void *>(buffer_ptr);

    saveBufferToFile(vPtr, fileTxt.length(), fpath);
}

scString grdPersQueueTask::findFreeFileName(const scString &fnameTpl, const scString &varPart)
{
    boost::filesystem::path p; 

    uint cnt = 1;
    bool bFound = false;
    scString newPart, fname, baseName;
    scString ext;

    for(uint i=0, epos = 1000; i != epos; i++) {
        newPart = dateTimeToNoSepStr(currentDateTime());
        if (i > 0) {
            newPart += toString(i);
        }  
        if (fnameTpl.find_first_of(varPart) != scString::npos) {
          fname = fnameTpl;
          strReplace(fname, varPart, newPart);
        } else {
          p = boost::filesystem::path(fnameTpl.c_str());
          ext = p.extension().generic_string();
          newPart += ext;

          //p.replace_extension(newPart.c_str());
          baseName = p.stem().string();
		  baseName += "." + newPart;
		  p.remove_filename();
		  p /= baseName;
		  
          fname = p.string();
        }
        if (!fileExists(fname)) {
            bFound = true;
            break;
        }
    }  
  
    if (!bFound)
      throw scError("Archive file name overflow");  

    return fname;
}

void grdPersQueueTask::reformatArchiveRows(const scDataNode &rows, scDataNode &output)
{
    output.setAsParent();
    std::auto_ptr<scDataNode> metaInfo(new scDataNode(ict_parent));

    metaInfo->setElementSafe("queue", scDataNode(getQueueName()));
    metaInfo->setElementSafe("export_dt", scDataNode(dateTimeToIsoStr(checkDataModule()->getCurrentDt())));
    output.addChild("meta", metaInfo.release());

    std::auto_ptr<scDataNode> body(new scDataNode());
    body->setAsList();
    body->copyChildrenFrom(rows);

    output.addChild("body", body.release());
}

void grdPersQueueTask::lockMessageList(const scDataNode &msgList)
{
    m_msgList.lockMessageList(msgList, checkDataModule()->getCurrentDt());
    checkDataModule()->lockMessageList(getQueueName(), msgList);
}

void grdPersQueueTask::unlockMessageList(const scDataNode &msgList)
{
    m_msgList.unlockMessageList(msgList, checkDataModule()->getCurrentDt());
    checkDataModule()->unlockMessageList(getQueueName(), msgList);
}

void grdPersQueueTask::cancelMessageList(const scDataNode &msgList)
{
    ulong64 msgId;
    for(uint i=0, epos = msgList.size(); i != epos; i++) {
        msgId = msgList[i].getUInt64("message_id");
        cancelMessage(msgId);
    }
}

bool grdPersQueueTask::cancelMessage(ulong64 msgId)
{
  changeMsgStatusAndLock(msgId, pqsHandled, 0);

  scDataNode error;

  m_msgList.setItemExecResult(msgId, SC_MSG_STATUS_USR_ABORT, scDataNode(""), error);
  checkDataModule()->setItemExecResult(getQueueName(), msgId, true, SC_MSG_STATUS_USR_ABORT, scDataNode(""), error);

  return true;
}

bool grdPersQueueTask::peekMessage(ulong64 msgId, scDataNode &output)
{
    return m_msgList.getItem(msgId, output);
}

bool grdPersQueueTask::peekMessage(const scString &ref, scDataNode &output)
{
    ulong64 msgId;
    if (m_msgList.findByRef(ref, msgId)) {
      return m_msgList.getItem(msgId, output);
    } else {
      return false;
    }
}

void grdPersQueueTask::exportMessages(const scString &fpath, grdPersQueueMsgStatus status, ulong64 msgId)
{
   scDataNode rows;
   checkDataModule()->getRowsForExport(getQueueName(), status, msgId, rows);
   saveArchiveToFile(fpath, rows);
}

void grdPersQueueTask::importMessages(const scString &fpath)
{
   scDataNode rows;
   scString fileTxt;

   readTextFileToString(fpath, fileTxt);
   dnSerializer serializer;
   serializer.convFromString(fileTxt, rows);
   if (rows.hasChild("body")) {
       scDataNode &body = rows["body"];
       scDataNode msgRec;
       scDataNode importRec;
       for(uint i=0, epos = body.size(); i != epos; i++) {
           body.getElement(i, msgRec);
           importRec = scDataNode(ict_parent);
           importRec.setElementSafe("msg_command", msgRec.getElement("command"));
           if (msgRec.hasChild("params"))
               importRec.setElementSafe("msg_params", msgRec.getElement("params"));
           if (msgRec.hasChild("reference"))
               importRec.setElementSafe("msg_ref", msgRec.getElement("reference"));
           
           addMessage(importRec);
       } // for
   } // if body
} // function

void grdPersQueueTask::registerQueue(const scString &aliasName, const scString &execAtAddr)
{
    scString sendToAddr;
    if (!execAtAddr.empty())
        sendToAddr = execAtAddr;
    else
        sendToAddr = getScheduler()->getOwnAddress().getAsString();

    scString usedAlias(aliasName);

    if (usedAlias.empty())
        usedAlias = scString("@pqueue.")+getQueueName();

    scString target = this->getOwnAddress("").getAsString();

    scDataNode params(ict_parent);
    params.setElementSafe("source", scDataNode(usedAlias));
    params.setElementSafe("target", scDataNode(target));
    params.setElementSafe("public", scDataNode(true));
    params.setElementSafe("direct_contact", scDataNode(true));

    getScheduler()->postMessage(sendToAddr, "core.reg_node", &params);
}

uint grdPersQueueTask::getMessageCount()
{
    return m_msgList.size();
}
