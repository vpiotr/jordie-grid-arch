/////////////////////////////////////////////////////////////////////////////
// Name:        HttpBridge.cpp
// Project:     grdLib
// Purpose:     Http bridge module & classes - for performing requests from
//              HTTP clients (like Python).
// Author:      Piotr Likus
// Modified by:
// Created:     22/12/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//std
#include <set>

// dtp
#include "dtp/dnode_serializer.h"

//sc
#include "perf/Log.h"
#ifdef _DEBUG
#include "perf/Timer.h"
#endif
//local
#include "grd/HttpBridge.h"

using namespace std;
using namespace mongoose;
using namespace dtp;
using namespace perf;

// ----------------------------------------------------------------------------
// local declarations
// ----------------------------------------------------------------------------
// constants
const uint DEF_HTTPB_PORT = 80;
const uint DEF_HTTPB_CLI_LIMIT = 100;
const uint DEF_HTTPB_INACT_TIMEOUT = 1000*60*30;
const uint DEF_HTTPB_WAIT_DELAY = 1000;
const uint DEF_HTTPB_RESPONSE_LIMIT = 100;
const uint DEF_HTTPB_MSG_TIMEOUT = 1000*60*10;
const scString DEF_HTTPB_PATH = "/";       /// default http access path  
const uint DEF_HTTPB_HTTPD_THREAD_CNT = 5; /// default number of threads for http deamon for http bridge

const uint HTTP_STATUS_OK           = 200;
const uint HTTP_STATUS_NO_CONTENT   = 204;
const uint HTTP_STATUS_BAD_REQUEST  = 400;
const uint HTTP_STATUS_NOT_FOUND    = 404;
const uint HTTP_STATUS_WRONG_METHOD = 405;
const uint HTTP_STATUS_CONFLICT     = 409;

const uint HTTPB_SESSION_STATUS_CLOSED = 0; 


// ----------------------------------------------------------------------------
// HttpBridgeModule
// ----------------------------------------------------------------------------
HttpBridgeModule::HttpBridgeModule(): scModule()
{
    m_managerTask = SC_NULL;
}

HttpBridgeModule::~HttpBridgeModule()
{
    if (m_managerTask != SC_NULL)
        m_managerTask->setParentModule(SC_NULL);
}

// -- module support --
scStringList HttpBridgeModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("httpb");
  return res;
}

int HttpBridgeModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;
  scString coreCmd = message->getCoreCommand();

  assert(message != SC_NULL);
  response.clearResult();

  if (
     (message->getInterface() == "httpb")
     )
  {   
  //init, listen, close, clear, get_status
    if (coreCmd == "init")
    {
      res = handleCmdInit(message, response);
    }  
  }
  
  response.setStatus(res);
  return res;
}

scTaskIntf *HttpBridgeModule::prepareTaskForMessage(scMessage *message)
{
  scTask *res = SC_NULL;
  scString coreCmd = message->getCoreCommand();
  
  if (
     (message->getInterface() == "httpb")
     )
  {   
  //init, listen, close, clear, get_status
    if (coreCmd == "init")
    {
      if (m_managerTask != SC_NULL)
          throw scError("Bridge already active");

      res = newManager(message);

      m_managerTask = dynamic_cast<HttpBridgeManagerTask *>(res);
    }  
  }

  return res;
}

int HttpBridgeModule::handleCmdInit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {   
    if (managerExists()) 
    {
   	res = SC_MSG_STATUS_ERROR;
        response.setError(scDataNode("Bridge already created"));
    } else { // no queue yet
   	res = SC_MSG_STATUS_TASK_REQ;        
    }      
  } // has children 
           
  return res;
}

bool HttpBridgeModule::managerExists()
{
    return (m_managerTask != SC_NULL);
}

HttpBridgeManagerTask *HttpBridgeModule::prepareManager()
{
    if (m_managerTask == SC_NULL)
        throw scError("Bridge manager not ready");
    return m_managerTask;
}

scTask *HttpBridgeModule::newManager(scMessage *message)
{

    std::auto_ptr<HttpBridgeManagerTask> mtask(new HttpBridgeManagerTask);

    scDataNode &params = message->getParams(); 
    scDataNode jparams;

    mtask->setParentModule(this);

    if (params.hasChild("params")) {
        if (params.getElementType("params") == vt_string) {
            dnSerializer serializer;
            serializer.convFromString(params.getString("params"), jparams);
        } else {
            jparams = params["params"];
        }
        params = jparams;
    }

    mtask->setPort(           params.getUInt(  "port",          DEF_HTTPB_PORT));
    mtask->setClientLimit(    params.getUInt(  "cli_limit",     DEF_HTTPB_CLI_LIMIT));
    mtask->setInactTimeout(   params.getUInt(  "inact_timeout", DEF_HTTPB_INACT_TIMEOUT));
    mtask->setMessageTimeout( params.getUInt(  "msg_timeout",   DEF_HTTPB_MSG_TIMEOUT));
    mtask->setResponseLimit(  params.getUInt(  "resp_limit",    DEF_HTTPB_RESPONSE_LIMIT));
    mtask->setWaitDelay(      params.getUInt(  "wait_delay",    DEF_HTTPB_WAIT_DELAY));
    mtask->setPath(           params.getString("path",          DEF_HTTPB_PATH));

    return mtask.release();
}

void HttpBridgeModule::clearManagerRef(scTask *manager)
{
    if (m_managerTask == manager) {
      m_managerTask = SC_NULL;
    }
}

// ----------------------------------------------------------------------------
// HttpBridgeManagerTask
// ----------------------------------------------------------------------------
HttpBridgeManagerTask::HttpBridgeManagerTask()
{
    m_lastSweepDt = 0;
    m_cliSessionData.setAsParent();
    m_cliMessagesIn.setAsParent();
    m_cliMessagesOut.setAsParent();
    m_cliResponseWaitInfo.setAsParent();
}

HttpBridgeManagerTask::~HttpBridgeManagerTask()
{
    if (m_parentModule != SC_NULL)
        m_parentModule->clearManagerRef(this);
}

void HttpBridgeManagerTask::setClientLimit(int value)
{
    m_clientLimit = value;
}

void HttpBridgeManagerTask::setPort(int value)
{
    m_port = value;
}

void HttpBridgeManagerTask::setInactTimeout(int value)
{
    m_inactTimeout = value;
}

void HttpBridgeManagerTask::setWaitDelay(int value)
{
    m_waitDelay = value;
}

void HttpBridgeManagerTask::setResponseLimit(int value)
{
    m_responseLimit = value;
}

void HttpBridgeManagerTask::setPath(const scString &value)
{
    m_path = value;
}

void HttpBridgeManagerTask::setMessageTimeout(int value)
{
    m_messageTimeout = value;
}

void HttpBridgeManagerTask::setParentModule(HttpBridgeModule *module)
{
    m_parentModule = module;
}

void HttpBridgeManagerTask::intInit()
{
    Log::addDebug("Before HTTP bridge start");
    m_service.reset(new HttpBridgeService);
    m_service->setRequestPath(m_path);
    m_service->setHandler(this);
    m_service->setOption("listening_ports", stringToStdString(toString(m_port)));
    m_service->setOption("num_threads", stringToStdString(toString(DEF_HTTPB_HTTPD_THREAD_CNT)));
    m_service->start();
    Log::addDebug("HTTP bridge started");
} 

void HttpBridgeManagerTask::intDispose()
{
    m_service->stop();
    m_service->setHandler(SC_NULL);
    for(HttpSessionTaskMap::iterator it = this->m_sessionTasks.begin(), epos = this->m_sessionTasks.end(); it != epos; ++it) {
        dynamic_cast<HttpBridgeSessionTask *>(it->second)->clearParentRef(this);
        it->second->requestStop();
    }
}

bool HttpBridgeManagerTask::isDaemon()
{
    return true;
}

bool HttpBridgeManagerTask::needsRun()
{
    return true;
}

int HttpBridgeManagerTask::intRun()
{
    if (is_cpu_time_elapsed_ms(m_lastSweepDt, 5000)) {
      checkTimeoutsProt();
      m_lastSweepDt = cpu_time_ms();
    }

    sendPendingOutMsgs();
    this->sleepFor(20);
    return 0;
}

/// - verify client inactivity timeouts
/// ? verify response timeouts
void HttpBridgeManagerTask::checkTimeoutsProt()
{
    boost::mutex::scoped_lock l(m_mutex); 

    std::set<scString> keysForDel;
    cpu_ticks lastDt;

    for(uint i=0, epos = m_cliSessionData.size(); i != epos; i++) {
        lastDt = m_cliSessionData[i].getUInt64("last_contact_dt");
        if (
              (lastDt == HTTPB_SESSION_STATUS_CLOSED)
              ||
              (
                (m_inactTimeout > 0) && is_cpu_time_elapsed_ms(lastDt, m_inactTimeout)
              )
           ) 
        {
            keysForDel.insert(m_cliSessionData.getElementName(i));
        }
    }

    for(std::set<scString>::const_iterator cit=keysForDel.begin(), epos = keysForDel.end(); cit != epos; ++cit) {
        intCloseSessionTask(*cit);
        intCloseSessionData(*cit);
        Log::addDebug("Close session executed");
    }
}

void HttpBridgeManagerTask::clearSessionRefProt(const scString &clientKey, HttpBridgeSessionTask *task)
{
    boost::mutex::scoped_lock l(m_mutex); 
    intCloseSessionData(clientKey);
}

void HttpBridgeManagerTask::intCloseSessionTask(const scString &clientKey)
{
    HttpSessionTaskMap::iterator it = this->m_sessionTasks.find(clientKey);
    if (it != this->m_sessionTasks.end()) {
        it->second->requestStop();
    }
}

void HttpBridgeManagerTask::intCloseSessionData(const scString &clientKey)
{
    HttpSessionTaskMap::iterator it = this->m_sessionTasks.find(clientKey);
    if (it != this->m_sessionTasks.end()) 
        this->m_sessionTasks.erase(it);

    scDataNode::size_type idx;

    idx = m_cliMessagesIn.indexOfName(clientKey);
    if (idx != scDataNode::npos)
      m_cliMessagesIn.eraseElement(idx);

    idx = m_cliMessagesOut.indexOfName(clientKey);
    if (idx != scDataNode::npos)
      m_cliMessagesOut.eraseElement(idx);
    
    idx = m_cliResponseWaitInfo.indexOfName(clientKey);
    if (idx != scDataNode::npos)
      m_cliResponseWaitInfo.eraseElement(idx);

    idx = m_cliSessionData.indexOfName(clientKey);
    if (idx != scDataNode::npos)
      m_cliSessionData.eraseElement(idx);
}

/// Purpose:
///   - used to send messages from HTTP client to grdLib nodes / tasks / services
/// Input: 
///   clikey       - optional - session ID
///   requestData  - one or more messages to be sent in form of "envelope" or "envelopes"
/// Output:
///   statusCode  
///     = bhsOk           - when everything was correct
///     = bhsWrongCliKey  - when session ID was incorrect (expired?)
///   responseData - empty
///   usedCliKey   - session ID that was used to handle request. it is non-empty when input session ID was empty and needs to be generated
/// Processing:
///   - lock session data
///   - prepare new session 
///     or (if session ID is non-empty)
///     find session buffer
///   - for each message:
///     - post message to sheduler as outgoing
///     - store information about sent message in internal session's buffer for future reference (timeout checking)
///   - unlock session data
void HttpBridgeManagerTask::putRequestProt(const scString &clientKey, const scDataNode &requestData, int &statusCode, scDataNode &responseData, scString &usedCliKey)
{
    scString realCliKey = clientKey;

    if (clientKey.empty()) {
        connectClientProt(usedCliKey);
        realCliKey = usedCliKey;
    } 
    
    boost::mutex::scoped_lock l(m_mutex); 

    if (!clientKey.empty())
    {
        if (!intCheckSessionExists(clientKey)) {
            statusCode = bhsWrongCliKey;
            return;
        }
    }

    if (requestData.hasChild("envelopes")) {
        for(uint i=0, epos = requestData["envelopes"].size(); i != epos; i++) {
            intPutRequest(realCliKey, requestData["envelopes"].getElement(i));
        }
        statusCode = bhsOk;
    } else {
       if (requestData.hasChild("envelope")) {
         intPutRequest(realCliKey, requestData["envelope"]);
         statusCode = bhsOk;
       } else {
         statusCode = bhsUknownError;
       }
    }
}

void HttpBridgeManagerTask::intPutRequest(const scString &clientKey, const scDataNode &envelopeData)
{
    m_cliMessagesOut[clientKey].addElement(envelopeData);
}

// send buffered messages from HTTP client to grdLib destinations
void HttpBridgeManagerTask::sendPendingOutMsgs()
{
    boost::mutex::scoped_lock l(m_mutex); 
    uint waitingOut = 0;
    std::auto_ptr<scDataNode> newSentInfo;
    scString sessionId;
    
    for (uint i=0, epos = m_cliMessagesOut.size(); i != epos; i++) {
        waitingOut += m_cliMessagesOut[i].size();
    }

    if (waitingOut == 0)
        return;

    int orgReqId, outReqId;

    for (uint i=0, epos = m_cliMessagesOut.size(); i != epos; i++) {
        if (!m_cliMessagesOut[i].empty()) {
            sessionId = m_cliMessagesOut.getElementName(i);
            HttpSessionTaskMap::const_iterator cit = m_sessionTasks.find(sessionId);

            if (cit != m_sessionTasks.end())
            for(uint j = 0, eposj = m_cliMessagesOut[i].size(); j != eposj; j++)
            {
                // send message j
                static_cast<HttpBridgeSessionTask *>(cit->second)->sendEnvelope(m_cliMessagesOut[i][j], orgReqId, outReqId);
                markSessionUsed(sessionId);
                if (outReqId >= 0) {
                  newSentInfo.reset(new scDataNode());
                  newSentInfo->setAsParent();
                  newSentInfo->addChild("out_req_id", new scDataNode(outReqId));
                  newSentInfo->addChild("org_req_id", new scDataNode(orgReqId));
                  m_cliResponseWaitInfo[sessionId].addChild(newSentInfo.release());
                }
            }

            m_cliMessagesOut[i].clear();
            m_cliMessagesOut[i].setAsList();
        }
    }

}

/// Purpose: 
/// - handle message received by session task from grdLib io channels
///   - buffer it for read by HTTP client
/// Processing
/// - find client session ID for a given message
///   - if not found - log error, continue processing
/// - check message type:
///   - if standard message:
///     - simply add to session queue of incoming messages (m_cliMessagesIn)
///   - if response: 
///     - decode message id using m_cliResponseWaitInfo (out_req_id, org_req_id)  
///     - add message to internal queue of incoming messages for a given session
///     - remove message information from response wait queue
void HttpBridgeManagerTask::putMessageInProt(const scString &clientKey, const scEnvelope &envelope)
{
    boost::mutex::scoped_lock l(m_mutex); 

    bool sessionExists = intCheckSessionExists(clientKey);
    if (!sessionExists) {
        Log::addError(scString("Session [")+clientKey+"] does not exists, message ignored");
        return;
    }

    std::auto_ptr<scDataNode> newEnvelope(new scDataNode);

    newEnvelope->setAsParent();
    newEnvelope->setElementSafe("sender", scDataNode(envelope.getSender().getAsString()));
    newEnvelope->setElementSafe("receiver", scDataNode(envelope.getReceiver().getAsString()));

    if (envelope.getTimeout() > 0) {
        newEnvelope->setElementSafe("timeout", scDataNode(envelope.getTimeout()));
    }

    newEnvelope->addChild("event", new scDataNode());
    (*newEnvelope)["event"].setAsParent();

    if (!envelope.getEvent()->isResponse())
    {   // message
        scMessage *message = dynamic_cast<scMessage *>(envelope.getEvent());
        if (envelope.getEvent()->getRequestId() >= 0)
            (*newEnvelope)["event"].addChild("request_id", new scDataNode(envelope.getEvent()->getRequestId()));
        (*newEnvelope)["event"].addChild("command", new scDataNode(message->getCommand()));
        if (!message->getParams().isNull()) 
            (*newEnvelope)["event"].addChild("params", new scDataNode(message->getParams()));
    } else {
        // response
        if (envelope.getEvent()->getRequestId() >= 0) {
           int orgReqId;
           if (extractRequestInfo(clientKey, envelope.getEvent()->getRequestId(), orgReqId)) {
               if (orgReqId >= 0)
                 (*newEnvelope)["event"].addChild("request_id", new scDataNode(orgReqId));
           }
        } 
        scResponse *response = dynamic_cast<scResponse *>(envelope.getEvent());
        (*newEnvelope)["event"].addChild("status", new scDataNode(
            response->getStatus()));
        if (!response->getError().isNull()) {
            (*newEnvelope)["event"].addChild("error", new scDataNode(response->getError()));
        }
        if (!response->getResult().isNull()) {
            (*newEnvelope)["event"].addChild("result", new scDataNode(response->getResult()));
        }
    }

    m_cliMessagesIn[clientKey].addChild(newEnvelope.release());
}

// return original request id using m_cliResponseWaitInfo (out_req_id, org_req_id)  
bool HttpBridgeManagerTask::extractRequestInfo(const scString &clientKey, int outRequestId, int &orgRequestId)
{
    bool res = false;
    uint foundPos = 0;
    scDataNode &sessionInfo = m_cliResponseWaitInfo[clientKey];

    for(uint i=0, epos = sessionInfo.size(); i != epos; i++) {
        if (sessionInfo[i].getInt("out_req_id") == outRequestId) {
            orgRequestId = sessionInfo[i].getInt("org_req_id");
            foundPos = i;
            res = true;
        }
    }

    if (res) {
        sessionInfo.eraseElement(foundPos);
    }

    return res;
}

/// Purpose:
/// - read messages waiting for HTTP reader
void HttpBridgeManagerTask::readMessagesInProt(const scString &clientKey, cpu_ticks waitDelay, uint rowsLimit, int &statusCode, scDataNode &responseData)
{
    bool sessionExists = checkSessionExistsProt(clientKey);

    if (!sessionExists) {
        statusCode = bhsWrongCliKey;
        return;
    }

    uint usedRowLimit = (rowsLimit > 0)?rowsLimit:this->m_responseLimit;
    cpu_ticks usedWaitDelay = (waitDelay > 0)?waitDelay:this->m_waitDelay;

    cpu_ticks startTime = cpu_time_ms();

    while (!is_cpu_time_elapsed_ms(startTime, usedWaitDelay) && responseData.isNull()) 
    {
      {
          boost::mutex::scoped_lock l(m_mutex); 
          if (intHasAnyMessagesFor(clientKey)) {
             intReadMessagesIn(clientKey, usedRowLimit, responseData);
          }
      }
      if (responseData.isNull()) {
         boost::this_thread::yield();
      }
    }

    if (responseData.isNull())
        statusCode = bhsNoContent;
    else
        statusCode = bhsOk;
}

bool HttpBridgeManagerTask::intHasAnyMessagesFor(const scString &cliKey)
{
    bool res = false;
    if (m_cliMessagesIn.hasChild(cliKey)) {
        if (m_cliMessagesIn[cliKey].size() > 0) { 
            res = true;
        }
    }
    return res;
}

/// Purpose:
/// - read messages from start of m_cliMessagesIn, up to a given limit and 
/// - erase them from buffer
void HttpBridgeManagerTask::intReadMessagesIn(const scString &cliKey, uint rowLimit, scDataNode &responseData)
{
    scDataNode &sessionData = m_cliMessagesIn[cliKey];
    responseData.addChild("response", new scDataNode());
    responseData["response"].setAsList();
    scDataNode &outData = responseData["response"];

    while(!sessionData.empty() && ((rowLimit == 0) || (outData.size() < rowLimit))) {
        outData.addElement(sessionData.getElement(0));
        sessionData.eraseElement(0);
    }
}

void HttpBridgeManagerTask::connectClientProt(scString &usedCliKey)
{
    if (m_clientLimit > 0)
    {
        boost::mutex::scoped_lock l(m_mutex);  
        if (m_cliSessionData.size() >= m_clientLimit)
            throw scError("Client limit reached");
    }
    
    cpu_ticks currentTime;
    bool bSessionFound;

    do {     
      currentTime = cpu_time_ms();
      usedCliKey = toString(currentTime);
      bSessionFound = checkSessionExistsProt(usedCliKey);
    } while (bSessionFound);

    addSession(usedCliKey);
}

void HttpBridgeManagerTask::closeSessionProt(const scString &clientKey, int &statusCode, scDataNode &responseData)
{
    boost::mutex::scoped_lock l(m_mutex);  
    markSessionClosed(clientKey);
}

void HttpBridgeManagerTask::addSession(const scString &clientKey)
{
    boost::mutex::scoped_lock l(m_mutex); 

    std::auto_ptr<HttpBridgeSessionTask> task(new HttpBridgeSessionTask(this, clientKey));
    m_sessionTasks.insert(std::make_pair(clientKey, task.get()));
    this->getScheduler()->addTask(task.release());

    m_cliSessionData.addChild(new scDataNode(clientKey));
    m_cliSessionData[clientKey].setAsParent();
    m_cliSessionData[clientKey].setElementSafe("last_contact_dt", scDataNode(static_cast<ulong64>(cpu_time_ms())));

    m_cliMessagesIn.addChild(new scDataNode(clientKey));
    m_cliMessagesIn[clientKey].setAsList();

    m_cliMessagesOut.addChild(new scDataNode(clientKey));
    m_cliMessagesOut[clientKey].setAsList();

    m_cliResponseWaitInfo.addChild(new scDataNode(clientKey));
    m_cliResponseWaitInfo[clientKey].setAsList();
}

void HttpBridgeManagerTask::markSessionUsed(const scString &clientKey)
{
    if (m_cliSessionData.hasChild(clientKey))
      m_cliSessionData[clientKey].setElementSafe("last_contact_dt", scDataNode(static_cast<ulong64>(cpu_time_ms())));
}

void HttpBridgeManagerTask::markSessionClosed(const scString &clientKey)
{
    if (m_cliSessionData.hasChild(clientKey))
      m_cliSessionData[clientKey].setElementSafe("last_contact_dt", scDataNode(HTTPB_SESSION_STATUS_CLOSED));
}

bool HttpBridgeManagerTask::checkSessionExistsProt(const scString &clientKey)
{
    if (clientKey.empty()) {
        return false;
    } else {
        boost::mutex::scoped_lock l(m_mutex); 
        return this->m_cliSessionData.hasChild(clientKey);
    }
}

bool HttpBridgeManagerTask::intCheckSessionExists(const scString &clientKey)
{
    if (clientKey.empty()) {
        return false;
    } else {
        return this->m_cliSessionData.hasChild(clientKey);
    }
}

// lock messages and if there are any incoming messages for a given client then return, otherwise unlock messages
// result = is there any message waiting? true/false
bool HttpBridgeManagerTask::tryLockForRead(const scString &clientKey)
{
    return false;
}

void HttpBridgeManagerTask::lockMessages()
{
}

void HttpBridgeManagerTask::unlockMessages()
{
}

// ----------------------------------------------------------------------------
// HttpBridgeSessionTask
// ----------------------------------------------------------------------------
HttpBridgeSessionTask::HttpBridgeSessionTask(HttpBridgeManagerTask *parent, const scString &cliKey): 
  m_parent(parent), m_cliKey(cliKey), scTask()
{   
}

HttpBridgeSessionTask::~HttpBridgeSessionTask()
{
    if (m_parent != SC_NULL) {
        m_parent->clearSessionRefProt(m_cliKey, this);
    }
}

bool HttpBridgeSessionTask::isDaemon()
{
    return true;
}

bool HttpBridgeSessionTask::needsRun()
{
    return false;
}

void HttpBridgeSessionTask::clearParentRef(HttpBridgeManagerTask *parent)
{
    if (m_parent == parent)
        m_parent = SC_NULL;
}

int HttpBridgeSessionTask::handleMessage(scEnvelope &envelope, scResponse &response)
{
    if (m_parent != SC_NULL) {
        m_parent->putMessageInProt(m_cliKey, envelope);
    }
    if (envelope.getEvent()->isResponse())
      return SC_MSG_STATUS_OK;
    else
      return SC_MSG_STATUS_FORWARDED;
}

int HttpBridgeSessionTask::handleResponse(scMessage *message, scResponse &response)
{
    scEnvelope envelope;
    envelope.setEvent(response.clone());
    if (m_parent != SC_NULL) {
        m_parent->putMessageInProt(m_cliKey, envelope);
    }
    return SC_MSG_STATUS_OK;
}

void HttpBridgeSessionTask::sendEnvelope(const scDataNode &envelopeData, int &orgRequestId, int &usedRequestId)
{
  bool isResponse = false;
  orgRequestId = usedRequestId = -1;
  
#ifdef _DEBUG
  Log::addDebug(scString("I'm about to send message: [")+
      envelopeData.dump()+
      "]");
#endif

  if (envelopeData.hasChild("event"))
      isResponse = envelopeData["event"].getBool("is_response", isResponse);
  
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  scMessageAddress receiverAddr(envelopeData.getString("receiver"));
  scMessageAddress senderAddr(getOwnAddress(receiverAddr.getProtocol()));
  uint timeout = envelopeData.getUInt("timeout", 0);

  envelopeGuard->setReceiver(receiverAddr);
  envelopeGuard->setSender(senderAddr);
  envelopeGuard->setTimeout(timeout);

  std::auto_ptr<scEvent> eventGuard;

  orgRequestId = envelopeData["event"].getInt("request_id", -1);

  if (!isResponse) {
      eventGuard.reset(new scMessage);
      static_cast<scMessage *>(eventGuard.get())->setCommand(envelopeData["event"].getString("command"));
      if (envelopeData["event"].hasChild("params"))
        static_cast<scMessage *>(eventGuard.get())->setParams(envelopeData["event"].getElement("params"));
      if (orgRequestId >= 0) {
          int outRequestId = getScheduler()->getNextRequestId(); 
          eventGuard->setRequestId(outRequestId);
          usedRequestId = outRequestId;
      }
  } else {
      eventGuard.reset(new scResponse);
      static_cast<scResponse *>(eventGuard.get())->setStatus(envelopeData["event"].getInt("status", -1));

      if (envelopeData["event"].hasChild("error"))
        static_cast<scResponse *>(eventGuard.get())->setError(envelopeData["event"].getElement("error"));

      if (envelopeData["event"].hasChild("result"))
        static_cast<scResponse *>(eventGuard.get())->setResult(envelopeData["event"].getElement("result"));

      if (orgRequestId >= 0) {
          eventGuard->setRequestId(orgRequestId);
          usedRequestId = orgRequestId;
      }
  }

  envelopeGuard->setEvent(eventGuard.release());
  getScheduler()->postEnvelope(envelopeGuard.release());
}

void HttpBridgeSessionTask::intDispose()
{
    if (m_parent != SC_NULL) {
        m_parent->clearSessionRefProt(m_cliKey, this);
        m_parent = SC_NULL;
    }
}

// ----------------------------------------------------------------------------
// HttpBridgeService
// ----------------------------------------------------------------------------
HttpBridgeService::HttpBridgeService(): m_handler(SC_NULL)
{
}

HttpBridgeService::~HttpBridgeService()
{
}

void HttpBridgeService::setHandler(HttpBridgeManagerTask* handler)
{
    m_handler = handler;
}

void HttpBridgeService::setRequestPath(const scString &value)
{
    m_requestPath = value;
    m_closeRequestPath = value + "/close";
}

bool HttpBridgeService::handleEvent(ServerHandlingEvent eventCode, MongooseConnection &connection, const MongooseRequest &request, MongooseResponse &response)
{
    bool res = false;

    if (eventCode == MG_NEW_REQUEST) { 
        if (
            (request.getUri() == m_requestPath)
            ||
            (request.getUri() == m_closeRequestPath)
           )
        {
            std::string clikey;
            if (!connection.getCookie("clikey", clikey))
                clikey = "";
            std::string closeReqTxt;
            bool closeReq = (request.getUri() == m_closeRequestPath);

            if (request.getRequestMethodCode() == rmcPost) {
              if (closeReq) 
                handleCloseRequest(clikey, request, response);
              else
                handlePostRequest(clikey, request, response);
            }
            else if (request.getRequestMethodCode() == rmcGet) {
              handleGetRequest(clikey, request, response);
            }
            else 
              returnRequestError(HTTP_STATUS_WRONG_METHOD, "", response);
        } else {
           returnErrorNotFound(request.getUri(), response);
        }
        
        // return "handled" for each path - disable directory browsing
        res = true;
    } 

    return res;
}

void HttpBridgeService::returnErrorNotFound(const scString &rpath, MongooseResponse &response)
{
    scString text = 
scString("")
+"<html><head>"
+"<title>404 Not Found</title>"
+"</head><body>"
+"<h1>Not Found</h1>"
+"<p>The requested URL "+rpath+" was not found on this server.</p>"
+"</body></html>";

    returnRequestError(HTTP_STATUS_NOT_FOUND, text, response);
}

/// input: 
///   clikey    - client session ID (optional)
///   request   - body contais JSON data with one or more envelopes
///   data type - application/json
/// output:
///   200       - when envelopes were successfully queued
///   409       - when non-existing session ID was provided on input     
///   <content> - empty 
void HttpBridgeService::handlePostRequest(const scString &clikey, const MongooseRequest &request, MongooseResponse &response)
{
#ifdef _DEBUG
    Timer::reset("handle-post");
    Timer::start("handle-post");
#endif
    scString usedCliKey;
    int statusCode;
    scString responseText;
    scString errorContext;

    if (m_handler == SC_NULL) {
      statusCode = bhsUknownError;
    } else {
        dnSerializer serializer;

        scString queryString = stdStringToString(request.readQueryString());
        scDataNode requestData, responseData;

        try {
            Log::addDebug(scString("Http Gate request arrived: ")+queryString);
            statusCode = bhsUknownError;

            errorContext = "DecodeMsg";
            serializer.convFromString(queryString, requestData);

            errorContext = "Put";
            m_handler->putRequestProt(clikey, requestData, statusCode, responseData, usedCliKey);

            if (!responseData.isNull()) {
                serializer.convToString(responseData, responseText);
            }
        } catch(...) {
          statusCode = bhsException;
        }
    } 

    if ((usedCliKey != clikey) && (!usedCliKey.empty()))
        response.setSetCookie("clikey", stringToStdString(usedCliKey));

    uint httpStatusCode;

    switch (statusCode) {
        case bhsOk:
            httpStatusCode = HTTP_STATUS_OK;
            returnRequestSuccess(httpStatusCode, responseText, response);
            break;
        case bhsWrongCliKey:
            httpStatusCode = HTTP_STATUS_CONFLICT;
            returnRequestErrorMsg(httpStatusCode, "Wrong session ID", response);
            break;
        default:
            httpStatusCode = HTTP_STATUS_BAD_REQUEST;
            returnRequestErrorMsg(httpStatusCode, scString("Unknown error [")+toString(statusCode)+"], context: ["+errorContext+"]", response);
    }

#ifdef _DEBUG
    Timer::stop("handle-post");
    cpu_ticks total = Timer::getTotal("handle-post");
    Log::addDebug(scString("HTTP-post-handle time: [")+toString(total)+"] ms");
#endif
};

void HttpBridgeService::handleCloseRequest(const scString &clikey, const MongooseRequest &request, MongooseResponse &response)
{
    int statusCode;
    scString responseText;
    scString errorContext;

    if (m_handler == SC_NULL) {
      statusCode = bhsUknownError;
    } else {
        dnSerializer serializer;
        scDataNode requestData, responseData;

        try {
            Log::addDebug(scString("Http Gate close session request arrived: ")+clikey);
            statusCode = bhsUknownError;

            errorContext = "Handler";
            m_handler->closeSessionProt(clikey, statusCode, responseData);

            if (!responseData.isNull()) {
                serializer.convToString(responseData, responseText);
            }
        } catch(...) {
          statusCode = bhsException;
        }
    } 

    uint httpStatusCode;

    switch (statusCode) {
        case bhsOk:
            httpStatusCode = HTTP_STATUS_OK;
            returnRequestSuccess(httpStatusCode, responseText, response);
            break;
        case bhsWrongCliKey:
            httpStatusCode = HTTP_STATUS_CONFLICT;
            returnRequestErrorMsg(httpStatusCode, "Wrong session ID", response);
            break;
        default:
            httpStatusCode = HTTP_STATUS_BAD_REQUEST;
            returnRequestErrorMsg(httpStatusCode, scString("Unknown error [")+toString(statusCode)+"], context: ["+errorContext+"]", response);
    }
}

void HttpBridgeService::handleGetRequest(const scString &clikey, const MongooseRequest &request, MongooseResponse &response)
{
#ifdef _DEBUG
    Timer::reset("handle-get");
    Timer::start("handle-get");
#endif
    scString usedCliKey;
    int statusCode;
    scString responseText;
    scString errorContext;

    if (m_handler == SC_NULL) {
      statusCode = bhsUknownError;
    } else {
        dnSerializer serializer;
        scDataNode requestData, responseData;
        uint waitDelay = 0;
        uint rowsLimit = 0;
        std::string sValue;

        if (request.getVar("wait_delay", sValue)) {
            waitDelay = stringToUIntDef(stdStringToString(sValue), 0);
        }

        if (request.getVar("rows_limit", sValue)) {
            rowsLimit = stringToUIntDef(stdStringToString(sValue), 0);
        }

        try {
            if (clikey.empty()) {
                errorContext = "Connect";
                m_handler->connectClientProt(usedCliKey);
                statusCode = bhsNoContent;
            } else {
                errorContext = "Read";
                statusCode = bhsUknownError;
                m_handler->readMessagesInProt(clikey, waitDelay, rowsLimit, statusCode, responseData);
                if (!responseData.isNull()) {
                    serializer.convToString(responseData, responseText);
                }
            }

        } catch(...) {
          statusCode = bhsException;
        }
    } 

    if ((usedCliKey != clikey) && (!usedCliKey.empty()))
        response.setSetCookie("clikey", stringToStdString(usedCliKey));

    uint httpStatusCode;

    switch (statusCode) {
        case bhsOk:
            httpStatusCode = HTTP_STATUS_OK;
            returnRequestSuccess(httpStatusCode, responseText, response);
            break;
        case bhsNoContent:
            httpStatusCode = HTTP_STATUS_NO_CONTENT;
            returnRequestSuccess(httpStatusCode, "", response);
            break;
        case bhsWrongCliKey:
            httpStatusCode = HTTP_STATUS_CONFLICT;
            returnRequestErrorMsg(httpStatusCode, "Wrong session ID", response);
            break;
        default:
            httpStatusCode = HTTP_STATUS_BAD_REQUEST;
            returnRequestErrorMsg(httpStatusCode, scString("Unknown error [")+toString(statusCode)+"], context: ["+errorContext+"]", response);
    }
#ifdef _DEBUG
    Timer::stop("handle-get");
    cpu_ticks total = Timer::getTotal("handle-get");
    Log::addDebug(scString("HTTP-get-handle time: [")+toString(total)+"] ms");
#endif
}

void HttpBridgeService::returnRequestSuccess(uint status, const scString &contentText, MongooseResponse &response)
{
    response.setStatus(status);
    response.setConnectionAlive(false);
    response.setCacheDisabled();
    response.addContent(stringToStdString(contentText));
    response.write();
}

void HttpBridgeService::returnRequestErrorMsg(uint status, const scString &msgText, MongooseResponse &response)
{
    scString text = 
scString("")
+"<html><head>"
+"<title>Request error</title>"
+"</head><body>"
+"<h1>"+msgText+"</h1>"
+"</body></html>";

    returnRequestError(status, text, response);
}

void HttpBridgeService::returnRequestError(uint status, const scString &contentText, MongooseResponse &response)
{
    response.setStatus(status);
    response.setConnectionAlive(false);
    response.setCacheDisabled();
    response.addContent(stringToStdString(contentText));
    response.write();
}
