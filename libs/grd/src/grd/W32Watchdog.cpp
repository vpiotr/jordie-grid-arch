/////////////////////////////////////////////////////////////////////////////
// Name:        W32Watchdog.cpp
// Project:     grdLib
// Purpose:     Implementation of watchdog module (Win32-specific). 
// Author:      Piotr Likus
// Modified by:
// Created:     25/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//std
#include <vector>

//wx
//#include "wx/file.h"
//#include <wx/utils.h> 

//base
#include "base/string.h"
#include "base/file_utils.h"

//perf
#include "perf/Log.h"

#include "base/rand.h"

//sc
#include "sc/defs.h"
#include "sc/utils.h"
#include "sc/proc/StartProcessThread.h"
#include "sc/proc/ptypes.h"
#include "sc/proc/process.h"

//grd
#include "grd/W32WatchDog.h"
#include "grd/MessageConst.h"

//win32
//#include <windows.h>

using namespace dtp;
using namespace perf;
using namespace proc;

//#ifdef SC_LOG_ENABLED
#define SC_WATCHDOG_LOG_ON
//#endif

class scW32WatchdogParentTask;

class scW32PostEchoEnumerator: public scProcessEnumerator {
public:
  scW32PostEchoEnumerator(scW32WatchdogParentTask *owner): scProcessEnumerator(), m_owner(owner) {};
  virtual ~scW32PostEchoEnumerator() {}  
  virtual void operator()(scProcessId pid) {m_owner->postEchoToChild(pid);};
protected:
  scW32WatchdogParentTask *m_owner;  
};

  
// ----------------------------------------------------------------------------
// scW32EchoHandler
// ----------------------------------------------------------------------------
class scW32EchoHandler: public scRequestHandler {
public:
  scW32EchoHandler(ulong64 pid): scRequestHandler() {m_childPid = pid;}  
  virtual ~scW32EchoHandler() {}
  //ignore result
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response) {};
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
  virtual bool handleException(const scError &error); 
protected:  
  void closeChild();
protected:
  ulong64 m_childPid;  
};

void scW32EchoHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
#ifdef SC_WATCHDOG_LOG_ON
  Log::addError("WATCHDOG echo req error for child ["+toString(m_childPid)+"], performing close");
#endif  
  closeChild(); 
}

bool scW32EchoHandler::handleException(const scError &error)
{
#ifdef SC_WATCHDOG_LOG_ON
  Log::addError("WATCHDOG echo error - exception: "+
    scString(error.what())+
    "for child ["+
    toString(m_childPid)+
    "], performing close");
#endif  
  closeChild();
  return true;
}

void scW32EchoHandler::closeChild()
{
  closeProcess(m_childPid);
  terminateProcess(m_childPid, SC_WATCHDOG_KILL_TIMEOUT);
}
  
// ----------------------------------------------------------------------------
// scW32WatchdogParentTask
// ----------------------------------------------------------------------------
scW32WatchdogParentTask::scW32WatchdogParentTask(
    const scString &childExecPath, 
    const scString &childExecParams,
    unsigned int startChildDelay, 
    uint aliveTimeout, 
    uint aliveInterval, 
    unsigned int childCount,
    scLimitSingleInstance *limitInstance,
    bool childTalk,
    const scString &workQueueAddr,
    const scString &workQueueName,
    const scString &regAddr,
    const scString &regName,
    const scString &childAddr
    ):  scTask()
{
  m_startChildDelay = (startChildDelay>0)?startChildDelay:SC_WATCHDOG_START_CHILD_DELAY;
  m_requiredChildCount = childCount;
  m_childExecPath = childExecPath;
  m_childExecParams = childExecParams;
  m_singleInstanceObj = limitInstance;
  m_childTalk = childTalk;
  m_workQueueAddr = workQueueAddr;
  m_workQueueName = workQueueName;
  m_regAddr = regAddr;
  m_regName = regName;
  m_childAddr = childAddr;
  m_aliveTimeout = (aliveTimeout > 0)?aliveTimeout:SC_WATCHDOG_MSG_TIMEOUT;
  m_aliveInterval = (aliveInterval > 0)?aliveInterval:startChildDelay;
}
    
scW32WatchdogParentTask::~scW32WatchdogParentTask()
{
  delete m_singleInstanceObj;
}

/// 1. Kill all running child processes skipping this process
/// 2. Start as many as required using thread
/// 3. Change status to "starting"
void scW32WatchdogParentTask::intInit()
{
  closeChildren();
  terminateChildren();
  if (m_requiredChildCount > 0) {
  //startChildren(m_requiredChildCount);
    startChildren(1);
    if (m_requiredChildCount <= 1) 
      sleepFor(m_startChildDelay);
  }    
}

void scW32WatchdogParentTask::intDispose()
{
  closeChildren();
  terminateChildren();
}

void scW32WatchdogParentTask::closeChildren()
{
  std::string strExecPath = m_childExecPath.c_str();  
  closeProcessByExec(const_cast<char *>(strExecPath.c_str()), true);
}

void scW32WatchdogParentTask::terminateChildren()
{
  int limit = SC_WATCHDOG_KILL_RETRIES;
  bool found;
  std::string strExecPath = m_childExecPath.c_str();
  
  do {
    found = terminateProcessByExec(const_cast<char *>(strExecPath.c_str()), 
      true, SC_WATCHDOG_KILL_TIMEOUT);
  } while ((--limit > 0) && (found));
  if (!limit)
    throw scError("Kill retry reached!");
}

// returns <true> if child was started
bool scW32WatchdogParentTask::checkChildCount()
{  
  bool res = false;
  std::string strExecPath = m_childExecPath.c_str();
  unsigned int count = countProcessByExec(const_cast<char *>(strExecPath.c_str()),  
    true);

  if (count < m_requiredChildCount)
  {
    //startChildren(m_requiredChildCount - count);    
    startChildren(1);    
    res = true;
  }
  
  return res;  
}    
  
void scW32WatchdogParentTask::sendMsgToEachChild() 
{
  std::string strExecPath = m_childExecPath.c_str();
  scW32PostEchoEnumerator enumerator(this);
  enumProcessByExec(const_cast<char *>(strExecPath.c_str()), true, &enumerator);    
}

void scW32WatchdogParentTask::postEchoToChild(ulong64 pid)
{
  scMessageAddress childAddr;
  std::auto_ptr<scMessage> msgGuard(new scMessage());
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope());
  
  calcChildAddress(pid, childAddr);
  msgGuard.get()->setCommand("core.echo");
  envelopeGuard->setEvent(msgGuard.release());
  envelopeGuard->setReceiver(childAddr);
  envelopeGuard->setTimeout(m_aliveTimeout); //
  envelopeGuard->getEvent()->setRequestId(
    getNextRequestId());
  
  getScheduler()->postEnvelope(envelopeGuard.release(), new scW32EchoHandler(pid));
}

void scW32WatchdogParentTask::calcChildAddress(ulong64 pid, scMessageAddress &output)
{
  output.clear();
  if (!m_childAddr.empty()) {
    scString fullChildAddr(m_childAddr);
    //fullChildAddr.Replace(SC_WATCHDOG_CHILD_ID_PATTERN, toString(pid), true);      
    strReplaceThis(fullChildAddr, SC_WATCHDOG_CHILD_ID_PATTERN, toString(pid), true);      
    output.setAsString(fullChildAddr);
  } else { 
    output.setNode(toString(pid));
    output.setProtocol(SC_WATCHDOG_PROTOCOL);
    if (!m_regAddr.empty())
      output.setProtocol(scMessageAddress(m_regAddr).getProtocol());
  }  
}

void scW32WatchdogParentTask::startChildren(unsigned int a_count)
{
  scString params;
  if (a_count > 0) 
  {
    calcExecParamsForChild(params);
#ifdef SC_WATCHDOG_LOG_ON
  Log::addError("WATCHDOG starting child ["+m_childExecPath+"]");
#endif  
    startChildCollection(a_count, m_childExecPath, params);
  }    
}

void scW32WatchdogParentTask::calcExecParamsForChild(scString &output)
{
  scString pattern;
  output = m_childExecParams;
  if (m_workQueueName.length() || m_regName.length())
  { 
    scString autoCmd;
    if (m_workQueueName.length())
    {
      autoCmd += "squeue.listen_at exec_at_addr='"+m_workQueueAddr+"',queue_name='"+m_workQueueName+"'";
    }
    if (m_regAddr.length())
    {
      if (autoCmd.length())
        autoCmd += ":";
      autoCmd += "core.reg_node_at exec_at_addr='"+m_regAddr+"'";
      if (m_regName.length())
        autoCmd += ",source_name='"+m_regName+"'";
    }
    pattern = SC_WATCHDOG_INIT_CMD_MACRO; 
    //output.Replace(pattern, autoCmd, true);      
    strReplaceThis(output, pattern, autoCmd, true);      
  }
}

void scW32WatchdogParentTask::startChildCollection(int count, const scString &cmd, const scString &params)
{
    std::vector<scStartProcessThread *> threadList;
    bool minimized, lowprior;

#ifdef SC_WATCHDOG_CHILDREN_MINIMIZED
    minimized = true;
#else
    minimized = false;
#endif

#ifdef SC_WATCHDOG_CHILDREN_LOWPRIOR
    lowprior = true;
#else
    lowprior = false;
#endif

    for(int i=1; i <= count; ++i)
    {
      threadList.push_back(new scStartProcessThread(cmd, params, minimized, lowprior));
    }
    
    for(std::vector<scStartProcessThread *>::iterator p=threadList.begin(); p != threadList.end(); ++p)
    {
      (*p)->Run();
    }
      
    scStartProcessThread::WaitForAll();
}

int scW32WatchdogParentTask::intRun()
{
  int res = 0;
  if (!isSleeping())
  {
    if (!checkChildCount())
    {
      if (m_childTalk) {
        sendMsgToEachChild();
      }  
      sleepFor(m_aliveInterval);
    } else {
      sleepFor(m_startChildDelay);
    }   
    res = 1;
  }  
  
  return res;
}

// ----------------------------------------------------------------------------
// scW32WatchdogChildTask
// ----------------------------------------------------------------------------
scW32WatchdogChildTask::scW32WatchdogChildTask(unsigned int delay): scTask()
{
  const uint DELAY_RANDOM_RANGE = 100;
  m_delay = delay;
  m_parentPid = getParentProcessId();
  sleepFor(randomUInt(m_delay, m_delay + DELAY_RANDOM_RANGE));
}

scW32WatchdogChildTask::~scW32WatchdogChildTask()
{
}

int scW32WatchdogChildTask::intRun()
{
  int res = 0;
  if (!isSleeping())
  {
    checkParent();
    sleepFor(m_delay);
    res = 1;
  }  
  
  return res;
}

void scW32WatchdogChildTask::checkParent()
{
  if (!processExists(m_parentPid)) {
#ifdef SC_WATCHDOG_LOG_ON
  Log::addWarning("WATCHDOG: parent process disappeared, performing close app");
#endif 
    try {
      Log::flush();
    } catch(...) {
    }
    closeProcess(GetCurrentProcessId());  
  }
}

// ----------------------------------------------------------------------------
// scW32WatchdogModule
// ----------------------------------------------------------------------------
scW32WatchdogModule::scW32WatchdogModule()
{
}

scW32WatchdogModule::~scW32WatchdogModule()
{
}

scStringList scW32WatchdogModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("watchdog");
  return res;
}

int scW32WatchdogModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;
  scString coreCmd = message->getCoreCommand();

  assert(message != SC_NULL);
  response.clearResult();

  if (
     (message->getInterface() == "watchdog")
     )
  {   
  //init
    if (coreCmd == "init")
    {
      res = handleCmdInit(message, response);
    }  
  }
  
  response.setStatus(res);
  return res;
}

scTaskIntf *scW32WatchdogModule::prepareTaskForMessage(scMessage *message)
{
  scTask *res = SC_NULL;
  scString coreCmd = message->getCoreCommand();
  scDataNode &params = message->getParams(); 
  
  if (
     (message->getInterface() == "watchdog")
     )
  {   
  //init
    if (coreCmd == "init")
    {
      scDataNode::size_type idxCount = params.getChildren().indexOfName("child_count");
      std::auto_ptr<scLimitSingleInstance> guard(new scLimitSingleInstance(SC_WATCHDOG_MUTEX_NAME));
      bool bInstanceFound = guard.get()->IsAnotherInstanceRunning();
      bool bParent = false;
      bool bAuto = false;
      
      if (!bInstanceFound)
      if (idxCount != scDataNode::npos) 
      { // parent?
        scDataNode &countParam = params["child_count"];
        int limit;
        scString limitStr = countParam.getAsString();
        
        if (limitStr == "auto")
        {
          bAuto = true;
          if (!bInstanceFound)
            bParent = true;
        } else { 
          limit = countParam.getAsInt();
          if (limit > 0)
            bParent = true;
        }    
      }
      
      if (bParent)
        res = prepareParentTask(message, guard.release());
      
      if (res == SC_NULL)
        res = prepareChildTask(message);    
    }  
  }  
  
  return res;
}

scTask *scW32WatchdogModule::prepareParentTask(scMessage *message, scLimitSingleInstance *limitInstance)
{
  scTask *res;
  //scString childExecPath, childExecParams;
  //unsigned int delay = SC_DEF_WATCHDOG_DELAY;
  //unsigned int childCount = 0;  
  //bool childTalk = true;
  scDataNode params;
  
  readParamsForParentTask(message, params);
  //childExecPath, childExecParams, delay, childCount, childTalk);
    
  res = new scW32WatchdogParentTask(
    params["exec_path"].getAsString(),
    params["exec_params"].getAsString(),
    params["delay"].getAsUInt(),
    params.getUInt("alive_timeout", 0),
    params.getUInt("alive_interval", 0),
    params["child_count"].getAsUInt(),
    limitInstance,
    params["child_talk"].getAsBool(),
    params["work_queue_addr"].getAsString(),
    params["work_queue_name"].getAsString(),
    params["reg_addr"].getAsString(),
    params["reg_name"].getAsString(),
    params["child_addr"].getAsString()
  );
  
#ifdef SC_WATCHDOG_LOG_ON
  Log::addText("watchdog initialized as parent");
#endif      
//    childExecPath, childExecParams,
//    delay, childCount, limitInstance, childTalk);  
  return res;  
}

void scW32WatchdogModule::readParamsForParentTask(
  scMessage *message, 
  scDataNode &outParams
//  scString &childExecPath, scString &childExecParams,
//  unsigned int &delay, unsigned int &childCount, bool &childTalk)
)
{
  scString childExecPath, childExecParams;
  scString workQueueName, workQueueAddr;
  scString regName, regAddr;
  unsigned int delay = SC_DEF_WATCHDOG_DELAY;
  unsigned int childCount = 0;  
  bool childTalk = true;
  
  scDataNode &params = message->getParams(); 

  scDataNode::size_type paramIdx;
  
  paramIdx = params.indexOfName("child_path");
  if (paramIdx != scDataNode::npos)
    childExecPath = params.getChildren().at(paramIdx).getAsString();
    
  paramIdx = params.indexOfName("child_params");
  if (paramIdx != scDataNode::npos)
    childExecParams = params.getChildren().at(paramIdx).getAsString();

  paramIdx = params.indexOfName("child_count");
  if (paramIdx != scDataNode::npos) 
  {
    scString limitStr = params["child_count"].getAsString();
    if (limitStr == "auto")
      childCount = wxThread::GetCPUCount();
    else  
      childCount = (unsigned int)params.getChildren().at(paramIdx).getAsInt();
  }    
    
  paramIdx = params.indexOfName("delay");
  if (paramIdx != scDataNode::npos)
    delay = (unsigned int)params.getChildren().at(paramIdx).getAsInt();
        
  if (params.hasChild("child_talk"))
    childTalk = (params["child_talk"].getAsString() == "true");  

  if (params.hasChild("work_queue_addr"))
    workQueueAddr = params["work_queue_addr"].getAsString();
  if (params.hasChild("work_queue_name"))
    workQueueName = params["work_queue_name"].getAsString();
  if (params.hasChild("reg_addr"))
    regAddr = params["reg_addr"].getAsString();
  if (params.hasChild("reg_name"))
    regName = params["reg_name"].getAsString();
  
  outParams.addChild("exec_path", new scDataNode(childExecPath));
  outParams.addChild("exec_params", new scDataNode(childExecParams));
  outParams.addChild("child_count", new scDataNode(childCount));
  outParams.addChild("delay", new scDataNode(delay));
  outParams.addChild("child_talk", new scDataNode(childTalk));
  outParams.addChild("work_queue_name", new scDataNode(workQueueName));
  outParams.addChild("work_queue_addr", new scDataNode(workQueueAddr));
  outParams.addChild("reg_name", new scDataNode(regName));
  outParams.addChild("reg_addr", new scDataNode(regAddr));
  outParams.addChild("child_addr", new scDataNode(params.getString("child_addr", "")));
  outParams.addChild("alive_timeout", new scDataNode(params.getUInt("alive_timeout", 0)));
  outParams.addChild("alive_interval", new scDataNode(params.getUInt("alive_interval", 0)));
}

scTask *scW32WatchdogModule::prepareChildTask(scMessage *message)
{
  scTask *res;
  unsigned int delay = SC_DEF_WATCHDOG_DELAY;
  
  readParamsForChildTask(message, delay);
    
  res = new scW32WatchdogChildTask(delay);  
#ifdef SC_WATCHDOG_LOG_ON
  Log::addText("watchdog initialized as child");
#endif        
  return res;  
}

void scW32WatchdogModule::readParamsForChildTask(
  scMessage *message, unsigned int &delay)
{
  scDataNode &params = message->getParams(); 

  if (params.hasChild("delay"))
    delay = (unsigned int)(params["delay"].getAsInt());
}

int scW32WatchdogModule::handleCmdInit(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_TASK_REQ;

  scDataNode &params = message->getParams(); 
  response.initFor(*message);        
  
  if (!params.empty()) {
    res = SC_MSG_STATUS_WRONG_PARAMS;
    
    scDataNode::size_type idxPath = params.indexOfName("child_path");
    scDataNode::size_type idxCount = params.indexOfName("child_count");
    
    if (idxCount == scDataNode::npos)
    { // check params for children
      res = SC_MSG_STATUS_TASK_REQ;
    }
    else if ((idxPath != scDataNode::npos) && (idxCount != scDataNode::npos))
    { // check params for parent
      int limit = -1;
      scDataNode &countParam = params["child_count"];
      scDataNode &pathParam = params["child_path"];
      scString fname = pathParam.getAsString();
      if (fileExists(fname)) 
      switch (countParam.getValueType()) 
      {
        case vt_int:
          limit = countParam.getAsInt();
          if (limit >= 0)
            res = SC_MSG_STATUS_TASK_REQ;
          break;
        case vt_string:
          limit = stringToIntDef(
            countParam.getAsString(), -1
          );
          if ((limit >= 0) || (countParam.getAsString() == "auto"))
            res = SC_MSG_STATUS_TASK_REQ;
          break;  
        default:
          // do nothing      
          break;
      } // switch type    
    } // if parent   
  } // has children 
           
  return res;
}

