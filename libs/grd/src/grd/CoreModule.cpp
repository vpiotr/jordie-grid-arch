/////////////////////////////////////////////////////////////////////////////
// Name:        CoreModule.cpp
// Project:     scLib
// Purpose:     Required module. Supports application execution commands.
// Author:      Piotr Likus
// Modified by:
// Created:     12/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// define to debug this module
#define CM_DEBUG
// define to debug forwarding
//#define CM_DEBUG_FORWARD

//std
#include <iostream>
#include <fstream>
//boost
#include <boost/lexical_cast.hpp>
//wx
#include <wx/utils.h> 
//dtp
#include "dtp/dnode_serializer.h"
//sc
#include "sc/dtypes.h"
#include "sc/utils.h"

//perf
#include "perf/Log.h"

//grd
#include "grd/CoreModule.h"
#include "grd/MessageConst.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace dtp;
using namespace perf;

// ----------------------------------------------------------------------------
// scForwardHandler
// ----------------------------------------------------------------------------
class scForwardHandler: public scRequestHandler {
public:
  scForwardHandler(int srcRequestId, const scEnvelope &envelope, scScheduler *scheduler);
  virtual ~scForwardHandler() {};
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
  scScheduler *getScheduler() {return m_scheduler;}
protected:
  scScheduler *m_scheduler;
  std::auto_ptr<scEnvelope> m_envelope;
  int m_srcRequestId;  
};

// ----------------------------------------------------------------------------
// scRegNodeHandler
// ----------------------------------------------------------------------------
class scRegNodeHandler: public scRequestHandler {
public:
  scRegNodeHandler(scScheduler *scheduler);
  virtual ~scRegNodeHandler() {};
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
  scScheduler *getScheduler() {return m_scheduler;}
protected:
  scScheduler *m_scheduler;
};

// ----------------------------------------------------------------------------
// Local class implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scForwardHandler
// ----------------------------------------------------------------------------
scForwardHandler::scForwardHandler(int srcRequestId, const scEnvelope &envelope, scScheduler *scheduler)
{
  m_srcRequestId = srcRequestId;
  m_scheduler = scheduler;
  m_envelope.reset(new scEnvelope(envelope));
}

void scForwardHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
#ifdef CM_DEBUG_FORWARD
   Log::addDebug("Forwarded message result received: "+a_message.getCommand());
#endif   
   std::auto_ptr<scEnvelope> envelopeGuard(
     new scEnvelope(
       scMessageAddress(m_envelope->getReceiver()), 
       scMessageAddress(m_envelope->getSender()), 
       new scResponse(a_response)));
       
   scEnvelope *outEnvelope = envelopeGuard.get();
   //copy requestId from original message
   outEnvelope->getEvent()->setRequestId(m_srcRequestId);
   //post response to original sender
   getScheduler()->postEnvelope(envelopeGuard.release());
}

void scForwardHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
   handleReqResult(a_message, a_response);
}

// ----------------------------------------------------------------------------
// scRegNodeHandler
// ----------------------------------------------------------------------------
scRegNodeHandler::scRegNodeHandler(scScheduler *scheduler): scRequestHandler()
{
  m_scheduler = scheduler;
}

void scRegNodeHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  scString newId;
  scDataNode result = a_response.getResult();
  if (!result.isNull()) {

    if (result.isContainer())
    {
      newId = result.getString("id");
    } else {
      newId = result.getAsString();
    } 
    
    if (!newId.empty()) {
        getScheduler()->setRegistrationId(newId);
    }    
  } 
  
  if (newId.empty())
  {
    Log::addWarning("reg_node - response arrived but w/o id");
  }
}

void scRegNodeHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  Log::addError(scString("reg_node error - ") + a_response.getError().getString("text", ""));
}

// ----------------------------------------------------------------------------
// Public class implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scCoreModule
// ----------------------------------------------------------------------------
scCoreModule::scCoreModule()
{
  m_scheduler = SC_NULL;
  m_commandParser = SC_NULL;
  m_onShutdown = SC_NULL;
  m_onRestart = SC_NULL;
  m_serializer.reset(new dnSerializer());
  static_cast<dnSerializer *>(m_serializer.get())->setCommentsEnabled(true);
}

scCoreModule::~scCoreModule()
{
}

int scCoreModule::handleMessage(const scEnvelope &envelope, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  bool handled = false;
  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());

  if (message->getInterface() == "core")
  { 
    if (message->getCoreCommand() == "forward")
    {   
      res = handleCmdForward(envelope, response);
      handled = true;
    } else if (message->getCoreCommand() == "advertise")
    {
      res = handleCmdAdvertise(envelope, response);
      handled = true;
    }
  }

  if (!handled)    
  {
    res = scModule::handleMessage(envelope, response);    
  }

  return res;
}

int scCoreModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;
  scString coreCmd = message->getCoreCommand();

  assert(message != SC_NULL);
  response.clearResult();

  if (
     (message->getInterface() == "core")
       ||
     (message->getInterface().length() == 0)
     )
  {   
    if (coreCmd == "if_equ")
    {
      res = handleCmdIfEqu(message, response);
    }  
    else if (coreCmd == "if_diff")
    {
      res = handleCmdIfDiff(message, response);
    }  
    else if (coreCmd == "echo")
    {
      res = handleCmdEcho(message, response);
    }  
    else if (coreCmd == "run")
    {
      res = handleCmdRun(message, response);
    }  
    else if (coreCmd == "run_cmd")
    {
      res = handleCmdRunCmd(message, response);
    }  
    else if (coreCmd == "set_option")
    {
      res = handleCmdSetOption(message, response);
    }  
    else if (coreCmd == "get_stats")
    {
      res = handleCmdGetStats(message, response);
    }  
    else if (coreCmd == "set_var")
    {
      res = handleCmdSetVar(message, response);
    }  
    else if (coreCmd == "reg_node")
    {
      res = handleCmdRegNode(message, response);
    }  
    else if (coreCmd == "reg_node_at")
    {
      res = handleCmdRegNodeAt(message, response);
    } 
    else if (coreCmd ==  "reg_map")
    {
      res = handleCmdRegMap(message, response);
    }  
    else if (coreCmd == "set_dispatcher")
    {
      res = handleCmdSetDispatcher(message, response);
    }  
    else if (coreCmd == "set_directory")
    {
      res = handleCmdSetDirectory(message, response);
    }  
    else if (coreCmd == "set_name")
    {
      res = handleCmdSetNodeName(message, response);
    }  
    else if (coreCmd == "import_env")
    {
      res = handleCmdImportEnv(message, response);
    }  
    else if (coreCmd == "flush_events")
    {
      res = handleCmdFlushEvents(message, response);
    }  
    else if (coreCmd == "create_node")
    {
      res = handleCmdCreateNode(message, response);
    }  
    else if (coreCmd == "shutdown_node")
    {
      res = handleCmdShutdownNode(message, response);
    }  
    else if (coreCmd == "restart_node")
    {
      res = handleCmdRestartNode(message, response);
    }  
    else if (coreCmd == "sleep")
    {
      res = handleCmdSleep(message, response);
    }  
    else if (coreCmd == "add_gate")
    {
      res = handleCmdAddGate(message, response);
    }  
  }
  
  response.setStatus(res);
  return res;
}

int scCoreModule::handleCmdEcho(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_OK;
  scString text;

  scDataNode &params = message->getParams(); 
  scString addr;  
  
  text = "echo";
  
  if (!params.empty())
  {
    text = params.getString("text", "");
  }  

  response.initFor(*message);        
  scDataNode resultData(ict_parent);
  resultData.setElementSafe("text", text);  
  response.setResult(resultData);  
  
  return res;
}

int scCoreModule::handleCmdRun(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString fname;
  scStringList contents;

  scDataNode &params = message->getParams(); 
  
  if (!params.empty()) {
    fname = params.getChildren().at(0).getAsString();
    if (!fname.empty()) {
      std::ifstream the_file( fname );
      if ( !the_file.is_open() )
      {
        throw scError("File open error: ["+fname+"]");
      }
      
     	std::string line;
     	
	    checkCommandParser();

	    while(std::getline(the_file,line)) {
        //m_commandParser->parseCommand(line);
	      contents.push_back(line); 
	    }      
	    
	    checkCommandParser();
      for(scStringList::iterator i=contents.begin(); i!=contents.end(); ++i){
        m_commandParser->parseCommand(*i);
      }   
	    
	    res = SC_RESP_STATUS_OK;
    } // fname filled    
  } // has children 
         
  response.initFor(*message);        
  
  return res;
}

int scCoreModule::handleCmdRunCmd(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString fname;

  scDataNode &params = message->getParams(); 
  
  if (!params.empty())
  {
    scString cmd;
    if (params.hasChild("command"))
      cmd = params.getString("command");
    else
      cmd = params.getString(0);

	  checkCommandParser();
    m_commandParser->parseCommand(cmd);
	    
	  res = SC_RESP_STATUS_OK;
  } // has correct children 
           
  return res;
}

int scCoreModule::handleCmdSetOption(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scString fname;

  scDataNode &params = message->getParams(); 
  
  if (!params.empty())
  {
    scString optionName, optionValue;
    if (params.hasChild("name") && params.hasChild("value"))
    {
      optionName = params.getString("name");
      optionValue = params.getString("value");
      setOption(optionName, optionValue);
  	  res = SC_RESP_STATUS_OK;
    }  	    
  } // has correct children 
           
  return res;
}

bool scCoreModule::setOption(const scString &optionName, const scString &optionValue)
{
  bool res = false;
  if (optionName == "show_processing_time")
  {
    setOption(sfLogProcTime, (optionValue == "true"));
    res = true;
  } else if (optionName == "log_messages")
  {
    setOption(sfLogMessages, (optionValue == "true"));
    res = true;
  } 
  return res;
}

void scCoreModule::setOption(scSchedulerFeature option, bool newValue)
{
  bool oldValue = checkScheduler()->isFeatureActive(option);
  if (oldValue != newValue)
  {
    if (newValue)
      checkScheduler()->setFeatures(checkScheduler()->getFeatures() | option);
    else  
      checkScheduler()->setFeatures(checkScheduler()->getFeatures() - option);
  }  
}

int scCoreModule::handleCmdForward(const scEnvelope &envelope, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
  scString fname;

#ifdef CM_DEBUG_FORWARD
  Log::addDebug("Forwarded message received from: "+envelope->getSender().getAsString()+", command: "+message->getCommand());
#endif
  scDataNode &params = message->getParams(); 
  
  if (params.hasChild("address") && params.hasChild("fwd_command"))
  {
    scString addr = params.getString("address");
    scString cmd = params.getString("fwd_command");
    bool genMsgId = params.getBool("gen_id", false);
    scDataNode fwdParams;
    scDataNode *ptrParams = SC_NULL;
    if (params.hasChild("fwd_params")) {
      fwdParams = params["fwd_params"];
      ptrParams = &fwdParams;
    } else if (params.hasChild("fwd_params_json"))
    {      
      scString str(params.getString("fwd_params_json"));
      m_serializer->convFromString(str, fwdParams);    
      ptrParams = &fwdParams;
    } 
    
    int newRequestId = SC_REQUEST_ID_NULL;
    if ((message->getRequestId() != SC_REQUEST_ID_NULL) || genMsgId)
      newRequestId = checkScheduler()->getNextRequestId();
    
    if (newRequestId != SC_REQUEST_ID_NULL) {  
      checkScheduler()->postMessage(addr, cmd, ptrParams, checkScheduler()->getNextRequestId(), 
        new scForwardHandler(message->getRequestId(), envelope, checkScheduler()));
    } else {
      checkScheduler()->postMessage(addr, cmd, ptrParams);
    }
    
	  res = SC_MSG_STATUS_FORWARDED;
  } // has correct children 
           
  return res;
}

///Syntax: core.advertise(role-name [, key])
int scCoreModule::handleCmdAdvertise(const scEnvelope &envelope, scResponse &response)
//int scCoreModule::handleCmdAdvertise(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 0) {      
      scString roleName;      
      scString searchKey;
      scString protocol = envelope.getSender().getProtocol();

      if (params.hasChild("role_name"))
         roleName = params.getString("role_name");
       else if (params.size() > 0)
         roleName = params.getString(0);

      if (params.hasChild("key"))
         searchKey = params.getString("key");
       else if (params.size() > 1)
         searchKey = params.getString(1);
#ifdef CM_DEBUG
      Log::addDebug("advertise: role-name=["+roleName+"], key: ["+searchKey+"]");             
#endif      
      scDataNode regEntries;
      scString searchName;

      if (roleName.find_first_of("@") == 0)
        searchName = roleName.substr(1);
      else
        searchName = roleName;  
        
      checkScheduler()->getRegistryEntriesForRole(protocol, searchName, searchKey, true, regEntries);
      if (regEntries.empty() && !checkScheduler()->isDirectoryNull())
      {
        res = SC_MSG_STATUS_FORWARDED;
        checkScheduler()->postMessage(checkScheduler()->getDirectoryAddr(), message->getCommand(), &params, checkScheduler()->getNextRequestId(), 
          new scForwardHandler(message->getRequestId(), envelope, checkScheduler()));        
      } else
      {     
        response.setResult(regEntries);      
        res = SC_MSG_STATUS_OK;
      }  
    }  
  }  
  return res;
}

int scCoreModule::handleCmdIfEqu(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  
  if (params.size() == 3) {
    res = SC_MSG_STATUS_OK;  
    scString value1 = params.getString(0);
    scString value2 = params.getString(1);
    scString cmd = params.getString(2);    
	  checkCommandParser();    
	  if (value1 == value2)
      m_commandParser->parseCommand(cmd);
  }
  
  return res;
}

int scCoreModule::handleCmdIfDiff(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  
  if (params.size() == 3) {
    res = SC_MSG_STATUS_OK;  
    scString value1 = params.getString(0);
    scString value2 = params.getString(1);
    scString cmd = params.getString(2);    
	  checkCommandParser();    
	  if (value1 != value2)
      m_commandParser->parseCommand(cmd);
  }
  
  return res;
}

int scCoreModule::handleCmdGetStats(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_OK;
  scString text = genStats();

  response.initFor(*message);        
  scDataNode resultData(ict_parent);
  resultData.setElementSafe("text", text);        
  response.setResult(resultData);  
  
  return res;
}

int scCoreModule::handleCmdShutdownNode(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_OK;
  response.initFor(*message);        
  performShutdown();  
  return res;
}

int scCoreModule::handleCmdRestartNode(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_OK;
  response.initFor(*message);        
  performRestart();  
  return res;
}

int scCoreModule::handleCmdSetNodeName(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (!params.empty()) {
      scString newName = params.getChildren().at(0).getAsString();
      checkScheduler()->setName(newName);
      res = SC_MSG_STATUS_OK;
    }  
  }  
  return res;
}

int scCoreModule::handleCmdSetVar(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_commandParser != SC_NULL) {
    if (params.size() > 1) {
      scString varName = params.getChildren().at(0).getAsString();
      scString varValue = params.getChildren().at(1).getAsString();
      m_commandParser->setVar(varName, varValue);
      res = SC_MSG_STATUS_OK;
    }  
  }  
  return res;
}

int scCoreModule::handleCmdRegNode(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 1) {
      scString srcName;
      scString targetName;
      
      if (params.hasChild("source"))
        srcName = params.getString("source");
      else  
        srcName = params.getString(0);
      
      if (params.hasChild("target"))
        targetName = params.getString("target");
      else    
        targetName = params.getString(1);
        
      bool publicEntry;
      bool directMode;
      
      publicEntry = params.getBool("public", false);
      directMode = params.getBool("direct_contact", false);
      uint shareTime = params.getUInt("share_time", 0);
      
      
#ifdef CM_DEBUG
      Log::addDebug("reg_node: source=["+srcName+"], target: ["+targetName+"]");             
#endif    
      scString newName;
      if (checkScheduler()->registerNodeAs(srcName, targetName, newName, publicEntry, directMode, shareTime))
      {
        if (srcName.empty())
          // response.setResult(scDataNode("id", newName));
          response.setResult(scDataNode(newName));
        res = SC_MSG_STATUS_OK;
      }  
    }  
  }  
  return res;
}

///Syntax: core.reg_node_at(exec_on_addr, source_name [,auto_source=true])
int scCoreModule::handleCmdRegNodeAt(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 0) {
      scString exec_at_addr;
      scMessageAddress exaddr, ownaddr; 
      scString source_name, target_name;
      scDataNode newParams;
      
      if (params.hasChild("exec_at_addr"))
        exec_at_addr = params["exec_at_addr"].getAsString();
      else  
        exec_at_addr = checkScheduler()->getDirectoryAddr();
      
      if (params.hasChild("source_name"))
        source_name = params["source_name"].getAsString();
      else {
        if (params.hasChild("auto_source") && (params.getString("auto_source") == "true"))
          source_name = checkScheduler()->getRegistrationId();  
      }
      exaddr = exec_at_addr;
      ownaddr = checkScheduler()->getOwnAddress(exaddr.getProtocol());
#ifdef CM_DEBUG
      Log::addDebug("reg_node_at: @=["+exec_at_addr+"], source=["+source_name+"], target: ["+ownaddr.getAsString()+"]");       
#endif
      newParams.addChild("source_name", new scDataNode(source_name));
      newParams.addChild("target_name", new scDataNode(ownaddr.getAsString()));
      if (params.hasChild("share_time"))
        newParams.addChild("share_time", new scDataNode(params.getString("share_time")));

      if (source_name.empty())
      {
        std::auto_ptr<scRequestHandler> requestHandlerGuard;
        requestHandlerGuard.reset(new scRegNodeHandler(checkScheduler()));      
        checkScheduler()->postMessage(exec_at_addr, "core.reg_node", &newParams, checkScheduler()->getNextRequestId(), requestHandlerGuard.release());
      } else {
        checkScheduler()->postMessage(exec_at_addr, "core.reg_node", &newParams);
      }  
      
      res = SC_MSG_STATUS_OK;
    }  
  }  
  return res;
}

int scCoreModule::handleCmdRegMap(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 1) {
      scString cmdFilter;
      scString targetName;
      int priority = 1;

      if (params.hasChild("cmd_filter"))
        cmdFilter = params.getString("cmd_filter");
      else  
        cmdFilter = params.getString(0);
      
      if (params.hasChild("target"))
        targetName = params.getString("target");
      else    
        targetName = params.getString(1);

      if (params.hasChild("priority"))
        priority = params.getInt("priority");
      else if (params.size() > 2)
        priority = params.getInt(2);
      
#ifdef CM_DEBUG
      Log::addDebug("reg_map: cmd_filter=["+cmdFilter+"], target: ["+targetName+"]");             
#endif    
      scString newName;
      checkScheduler()->registerCommandMap(cmdFilter, targetName, priority);
      res = SC_MSG_STATUS_OK;
    }  
  }  
  return res;
}

int scCoreModule::handleCmdSetDispatcher(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 0) {
      scString addr;
      scDataNode newParams;
      
      if (params.hasChild("address"))
        addr = params.getString("address");
      else  
        addr = params.getString(0);

      checkScheduler()->setDispatcher(addr);
      res = SC_MSG_STATUS_OK;
    }  
  }  
  return res;
}

int scCoreModule::handleCmdSetDirectory(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 0) {
      scString addr;
      scDataNode newParams;
      
      if (params.hasChild("address"))
        addr = params.getString("address");
      else  
        addr = params.getString(0);

      checkScheduler()->setDirectoryAddr(addr);
      res = SC_MSG_STATUS_OK;
    }  
  }  
  return res;
}

int scCoreModule::handleCmdImportEnv(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_commandParser != SC_NULL) {
    if (params.size() > 0) {
      scString varName = params.getChildren().at(0).getAsString();
      char *env = getenv(varName.c_str());
      if (env) {
        m_commandParser->setVar(varName, scString(env));
        res = SC_MSG_STATUS_OK;
      } else {
        res = SC_MSG_STATUS_ERROR;
      } 
    }  
  }  
  return res;
}


int scCoreModule::handleCmdFlushEvents(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_ERROR;
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    checkScheduler()->flushEvents();
    res = SC_MSG_STATUS_OK;
  }  
  return res;
}

int scCoreModule::handleCmdSleep(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  uint timems = 5;

  if (params.size() > 0) {
    timems = params.getUInt(0);
  }  
  wxMilliSleep(timems);
  res = SC_MSG_STATUS_OK;
  return res;
}

int scCoreModule::handleCmdCreateNode(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 
  response.initFor(*message);        

  if (m_scheduler != SC_NULL) {
    if (params.size() > 0) {
      scString className, nodeName, nodeCountTxt;
      
      className = params.getChildren().at(0).getAsString();
      if (params.size() > 1)
      {
        nodeCountTxt = params.getChildren().at(1).getAsString();
        //nodeCountTxt = nodeCountTxt.Trim();
        strTrimThis(nodeCountTxt);
        if (params.size() > 2)
          nodeName = params.getChildren().at(2).getAsString();
      }    
      
      int nodeCount = 1;
      
      if (!nodeCountTxt.empty())
        nodeCount = boost::lexical_cast< int >( std::string(nodeCountTxt) );    
       
      if (nodeCount < 0)
        nodeCount = 0;  
      
      checkScheduler()->createNodes(className, nodeCount, nodeName);
      res = SC_MSG_STATUS_OK;
    } // params exist 
  } // scheduler assigned 
  return res;
}

int scCoreModule::handleCmdAddGate(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;
  scDataNode &params = message->getParams(); 

  if (m_scheduler != SC_NULL) {
    if (params.size() > 1) {
      bool dirInput = (params.getString(0) == scString("input"));      
      scString protocol(params.getString(1));
      scDataNode extraParams;
      
      if (!protocol.empty())
      {        
        for(uint i=2, epos = params.size(); i != epos; i++)
          extraParams.addChild(new scDataNode(params.getElement(i)));

        scGateFactoryColn::const_iterator it = m_gateFactoryColn.find(protocol);

        if (it != m_gateFactoryColn.end()) {
          std::auto_ptr<scMessageGate> gateGuard(it->second->createGate(dirInput, extraParams, protocol));
          if (dirInput)
            checkScheduler()->addInputGate(gateGuard.release());
          else  
            checkScheduler()->addOutputGate(gateGuard.release());
          res = SC_MSG_STATUS_OK;
        } else {
          res = SC_MSG_STATUS_ERROR;
        }  
      }
    }  
  }  
  return res;
}

scStringList scCoreModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("core");
  return res;
}

void scCoreModule::registerGateFactory(const scString &protocol, scGateFactory *factory)
{
  scString pname(protocol); 
  m_gateFactoryColn.insert(pname, factory);
}

void scCoreModule::setCommandParser(scCommandParser *parser)
{
  m_commandParser = parser;
}

void scCoreModule::checkCommandParser()
{
  if (m_commandParser == SC_NULL)
    throw scError("Command parser not assigned");
}

void scCoreModule::setOnShutdown(scNoParamFunctor *functor)
{
  m_onShutdown = functor;
}

scNoParamFunctor *scCoreModule::getOnShutdown()
{
  return m_onShutdown;
}

void scCoreModule::setOnRestart(scNoParamFunctor *functor)
{
  m_onRestart = functor;
}

scNoParamFunctor *scCoreModule::getOnRestart()
{
  return m_onRestart;
}

scScheduler *scCoreModule::checkScheduler()
{
  if (m_scheduler == SC_NULL)
    throw scError("Scheduler not assigned!");
  return dynamic_cast<scScheduler *>(m_scheduler);  
}
    
void scCoreModule::performRestart()
{
  if (m_onRestart != SC_NULL)
    (*m_onRestart)();
}    

void scCoreModule::performShutdown()
{
  if (m_onShutdown != SC_NULL)
    (*m_onShutdown)();
}    

scString scCoreModule::genStats()
{
  scString res;
  checkScheduler();
  int taskCnt, moduleCnt, gateCnt;
  
  checkScheduler()->getStats(taskCnt, moduleCnt, gateCnt);
  res = "Statistics for: ["+ (checkScheduler()->getName())+"]\n"+
    "- number of tasks: "+toString(taskCnt)+"\n"+
    "- number of modules: "+toString(moduleCnt)+"\n"+
    "- number of gates: "+toString(gateCnt);    
    
  return res;  
}
