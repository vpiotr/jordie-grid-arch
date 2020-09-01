/////////////////////////////////////////////////////////////////////////////
// Name:        PersQueue.h
// Project:     grdLib
// Purpose:     Transactional queue which stores messages in database.
// Author:      Piotr Likus
// Modified by:
// Created:     05/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDPQUEUE_H__
#define _GRDPQUEUE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file PeristentQueue.h
///
/// Persistent queue
//  Automatic processing:
//  - restart:
//    - load all messages except "for purge"
//    - unlock & restart sent messages
//    - restart errors
//    - restart unanswered replies
//  - search for "ready" messages and try to activate them
//    - when exec addr is not empty
//    - after some interval
//  - handle handling timeout
//    - when no exec addr = using daemon "checkOutdatedExecs"
//    - when exec addre specified - using message timeout
//  - retry errors (exec, reply) after some time (after updated_dt)
//  - handle storage timeout
//  - perform "purge" process
//    - optional - archive
//    - delete
//  - registration
//    - control
//    - input
//  - direct control
//  - forward message using virtual address to queue manager task
//  + direct input message handling
//    - in similar way as in "put" but:
//    - params are used for msg-params
//    - msg-id is used for msg-ref
//  + post reply when message was handled
//    + on error put msg to status "reply error" for some time
//    + on success change msg status to "purge"

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "sc/DbSqlite.h"
#include "grd/core.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
enum grdPersQueueMsgStatus {
    pqsUndef = 0,
    pqsReady = 1,
    pqsForPurge = 2,
    pqsLocked = 3,
    pqsSent = 4,
    pqsHandled = 5,
    pqsReplySent = 6,
    pqsExecError = 7,
    pqsReplyError = 8
};

typedef std::list<scTask *> grdPersQueueTaskPtrList;

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class grdPersQueueDataModule
{
public:
    grdPersQueueDataModule(scDbBase *db);
    virtual ~grdPersQueueDataModule();
    bool isSchemaReady();
    void initSchema();
    ulong64 initQueue(const scString &qname, bool noData);
    void defineQueue(const scString &qname, const scDataNode &params);
    bool undefineQueue(const scString &qname);
    bool dropQueue(const scString &qname);
    ulong64 insertMessage(const scString &queueName, const scString &command, const scDataNode &params, const scDataNode &ref, 
        const scString &replyCmd, uint status);
    void setMsgStatusAndLock(scString &queueName, ulong64 msgId, uint status, ulong64 newLockId);
    bool setMsgStatusAndUnlock(scString &queueName, ulong64 msgId, uint status, ulong64 lockId);
    void setMsgStatus(scString &queueName, ulong64 msgId, uint status);
    scDateTime getCurrentDt();
    bool setItemExecResult(scString &queueName, ulong64 itemId, bool incError, int replyStatus, const scDataNode &result, const scDataNode &error);
    void loadMessages(const scString &queueName, scDataNode &msgList);
    ulong64 loadNextLockId(const scString &queueName, ulong64 defValue);
    bool saveNextLockId(const scString &queueName, ulong64 value);
    bool lockForPurge(const scString &queueName, ulong64 lockId);
    bool deleteForPurge(const scString &queueName);
    void selectLockedForPurge(const scString &queueName, ulong64 lockId, scDataNode &output);
    bool deleteLockedForPurge(const scString &queueName, ulong64 lockId);
    bool lockMessageList(const scString &queueName, const scDataNode &msgList);
    bool unlockMessageList(const scString &queueName, const scDataNode &msgList);
    void getRowsForExport(const scString &queueName, grdPersQueueMsgStatus status, ulong64 msgId, scDataNode &output);
    bool loadQueueConfig(const scString &baseName, scDataNode &output);
protected:
    void prepareDbStructure(); 
    void executeMultiSql(const scString &sqlText);
    bool recordExists(const scString &sql, scDataNode *params);
    scString getQueueMsgTableName(const scString &qname);
    void addQueueMsgTable(const scString &qname);
    ulong64 checkQueueId(const scString &queueName);
    bool getQueueId(const scString &queueName, ulong64 &qid);
    bool deleteQueueConfig(uint queueId);
    void defineQueueParam(uint queueId, const scDataNode &param, const scString &paramName);
    bool deleteQueue(uint queueId);
    bool dropQueueMsgTable(const scString &queueName);
    ulong64 insertMessageToTab(const scString &tabName, const scString &command, const scDataNode &params, const scDataNode &ref, 
        const scString &replyCmd, uint status);
    scString getCurrentDtAsIsoText();
    void prepareDateTimeField(scDataNode &row, const scString &fieldName);
    bool lockMessage(const scString &queueName, ulong64 msgId);
    bool unlockMessage(const scString &queueName, ulong64 msgId);
protected:
    scDbBase *m_db;    
};

class grdPersQueueTask;

class grdPersQueueModule: public scModule 
{
public:
    grdPersQueueModule();
    virtual ~grdPersQueueModule();
    virtual void init();
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
    virtual scStringList supportedInterfaces() const;
    scDbBase *getDatabase();
    grdPersQueueDataModule *checkDataModule();
    void setDbPath(const scString &path);
    scString getDbPath();
    void clearTaskRef(scTask *ref);
protected:
    void initDatabase();
    void clearManagerLinks();
    virtual grdPersQueueDataModule *newDataModule();
    bool isQueueModuleMessage(const scString &coreCmd);
    scTask *findQueue(const scString &name);
    bool queueExists(const scString &name);
    scTask *checkQueueTask(const scString &name);
    int forwardQueueMsg(scMessage *message, scResponse &response);
    //-- commands
    int handleCmdInit(scMessage *message, scResponse &response);
    int handleCmdOpen(scMessage *message, scResponse &response);
    int handleCmdClose(scMessage *message, scResponse &response);
    int handleCmdDrop(scMessage *message, scResponse &response);
    int handleCmdQList(scMessage *message, scResponse &response);
    int handleCmdDefine(scMessage *message, scResponse &response);
    int handleCmdUndefine(scMessage *message, scResponse &response);
    //--
    scTask *prepareQueueManager(scMessage *message);
    scTask *newQueueManagerTask(const scString &queueName);
    void getQueueList(scDataNode &output);
protected:
    std::auto_ptr<scDbBase> m_db;    
    scString m_dbPath;
    std::auto_ptr<grdPersQueueDataModule> m_dataModule;
private:
    grdPersQueueTaskPtrList m_managers;
};


#endif // _GRDPQUEUE_H__