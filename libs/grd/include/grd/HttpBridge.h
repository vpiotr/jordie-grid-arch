/////////////////////////////////////////////////////////////////////////////
// Name:        HttpBridge.h
// Project:     grdLib
// Purpose:     Http bridge module & classes - for performing requests from
//              HTTP clients (like Python).
// Author:      Piotr Likus
// Modified by:
// Created:     22/12/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _GRDHTTPBRIDGE_H__
#define _GRDHTTPBRIDGE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file HttpBridge.h
///
/// Http bridge module

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
//boost
#include <boost/thread.hpp>

//libs
#include "mongcpp.h"

//sc
#include "sc/dtypes.h"
//grd
#include "grd/core.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

class HttpBridgeManagerTask;

enum HttpBridgeHandlerStatus {
    bhsOk = 1,
    bhsWrongCliKey = 2,
    bhsUknownError = 3,
    bhsException = 4,
    bhsNoContent = 5
};

// ----------------------------------------------------------------------------
// HttpBridgeService
// ----------------------------------------------------------------------------
class HttpBridgeService: public mongoose::MongooseServer {
public:
    HttpBridgeService();
    virtual ~HttpBridgeService();
    void setHandler(HttpBridgeManagerTask* handler);
    void setRequestPath(const scString &value);
protected:
    virtual bool handleEvent(mongoose::ServerHandlingEvent eventCode, mongoose::MongooseConnection &connection, const mongoose::MongooseRequest &request, mongoose::MongooseResponse &response); 
    void handlePostRequest(const scString &clikey, const mongoose::MongooseRequest &request, mongoose::MongooseResponse &response);
    void handleCloseRequest(const scString &clikey, const mongoose::MongooseRequest &request, mongoose::MongooseResponse &response);
    void handleGetRequest(const scString &clikey, const mongoose::MongooseRequest &request, mongoose::MongooseResponse &response);
    void returnRequestSuccess(uint status, const scString &contentText, mongoose::MongooseResponse &response);
    void returnRequestErrorMsg(uint status, const scString &msgText, mongoose::MongooseResponse &response);
    void returnErrorNotFound(const scString &rpath, mongoose::MongooseResponse &response);
    void returnRequestError(uint status, const scString &contentText, mongoose::MongooseResponse &response);
protected:
    HttpBridgeManagerTask* m_handler;
    scString m_requestPath;
    scString m_closeRequestPath;
};

class HttpBridgeModule;
class HttpBridgeSessionTask;
typedef std::map<scString, scTask *> HttpSessionTaskMap;

// ----------------------------------------------------------------------------
// HttpBridgeManagerTask
// ----------------------------------------------------------------------------
class HttpBridgeManagerTask: public scTask {
public:
    // --- construct
    HttpBridgeManagerTask();
    virtual ~HttpBridgeManagerTask();
    // --- properties
    void setClientLimit(int value);
    void setPort(int value);
    void setInactTimeout(int value);
    void setWaitDelay(int value);
    void setResponseLimit(int value);
    void setPath(const scString &value);
    void setMessageTimeout(int value);
    void setParentModule(HttpBridgeModule *module);
    void clearSessionRefProt(const scString &clientKey, HttpBridgeSessionTask *task);
    // --- run
    virtual bool isDaemon();    
    virtual bool needsRun();
    void putRequestProt(const scString &clientKey, const scDataNode &requestData, int &statusCode, scDataNode &responseData, scString &usedCliKey);
    void putMessageInProt(const scString &clientKey, const scEnvelope &envelope);
    void readMessagesInProt(const scString &clientKey, cpu_ticks waitDelay, uint rowsLimit, int &statusCode, scDataNode &responseData);
    void connectClientProt(scString &usedCliKey);
    void closeSessionProt(const scString &clientKey, int &statusCode, scDataNode &responseData);
    bool checkSessionExistsProt(const scString &clientKey);
    void addSession(const scString &clientKey);
protected:
    bool tryLockForRead(const scString &clientKey);
    void lockMessages();
    void unlockMessages();
    virtual void intInit(); 
    virtual void intDispose();
    virtual int intRun();
    bool intHasAnyMessagesFor(const scString &cliKey);
    void intReadMessagesIn(const scString &cliKey, uint rowLimit, scDataNode &responseData);
    void intPutRequest(const scString &clientKey, const scDataNode &envelopeData);
    void checkTimeoutsProt();
    void intCloseSessionData(const scString &clientKey);
    void intCloseSessionTask(const scString &clientKey);
    void sendPendingOutMsgs();
    bool intCheckSessionExists(const scString &clientKey);
    bool extractRequestInfo(const scString &clientKey, int outRequestId, int &orgRequestId);
    void markSessionUsed(const scString &clientKey);
    void markSessionClosed(const scString &clientKey);
protected:
    uint m_clientLimit;
    uint m_port;
    uint m_inactTimeout;
    uint m_waitDelay;
    uint m_responseLimit;
    uint m_messageTimeout;
    uint m_nextClientId;
    cpu_ticks m_lastSweepDt;
    scString m_path;
    // protected: begin
    scDataNode m_cliMessagesIn;       /// messages[client-key] arrived from other nodes / tasks, waiting to be read by a HTTP client
    scDataNode m_cliMessagesOut;      /// messages[client-key] waiting to be sent by session tasks
    scDataNode m_cliResponseWaitInfo; /// [client-key] information about responses to be received: (client-as-key, message: send_dt, org message id, out message id) 
    scDataNode m_cliSessionData;      /// session information [client-key]: last contact dt
    HttpSessionTaskMap m_sessionTasks;
    // protected: end
    boost::mutex m_mutex;
    HttpBridgeModule *m_parentModule;
    std::auto_ptr<HttpBridgeService> m_service;
};


// ----------------------------------------------------------------------------
// HttpBridgeSessionTask
// ----------------------------------------------------------------------------
/// task for session-specific communication
class HttpBridgeSessionTask: public scTask {
public:
    HttpBridgeSessionTask(HttpBridgeManagerTask *parent, const scString &cliKey);
    virtual ~HttpBridgeSessionTask();
    void clearParentRef(HttpBridgeManagerTask *parent);
    virtual bool isDaemon();    
    virtual bool needsRun();
    virtual int handleMessage(scEnvelope &envelope, scResponse &response);    
    virtual int handleResponse(scMessage *message, scResponse &response);
    virtual void sendEnvelope(const scDataNode &envelopeData, int &orgRequestId, int &usedRequestId);
protected:
    virtual void intDispose(); 
protected:
    const scString m_cliKey;
    HttpBridgeManagerTask* m_parent;
};

// ----------------------------------------------------------------------------
// HttpBridgeModule
// ----------------------------------------------------------------------------
// module for handling bridge management messages
class HttpBridgeModule: public scModule {
public:
    // -- construct --
    HttpBridgeModule();
    virtual ~HttpBridgeModule();
    // -- module support --
    virtual scStringList supportedInterfaces() const;
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
    void clearManagerRef(scTask *manager);
    // --- commands ---
protected:
    // --- commands ---
    int handleCmdInit(scMessage *message, scResponse &response);
    // --- other ---
    HttpBridgeManagerTask *prepareManager();
    scTask *newManager(scMessage *message);
    bool managerExists();
protected:
    HttpBridgeManagerTask *m_managerTask;
};


#endif // _GRDHTTPBRIDGE_H__