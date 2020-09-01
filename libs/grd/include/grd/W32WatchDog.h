/////////////////////////////////////////////////////////////////////////////
// Name:        W32Watchdog.h
// Project:     scLib
// Purpose:     Watchdog module (Win32-specific). 
//              Maintains list of child processes.
// Author:      Piotr Likus
// Modified by:
// Created:     25/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _scW32WatchdogModule_H__
#define _scW32WatchdogModule_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file W32Watchdog.h
///
/// Watchdog interface implementation.
/// These classes control parent-child process relation cardinality.
/// User specifies number of required child processes and it is controlled here.
/// This file contains classes for watchdog.* interface support for Win32
/// platform.
/// 
/// watchdog.* commands:
/// - init(exec_path, child_params, child_count, delay): start watchdog task
///   - child_path: set path to child executable
///   - child_params: set command line parameters for child processes
///     - if includes macro #init_commands# - it will be replaced by init commands
///   - child_count: set number of children to maintain (=0 for child process)
///     - if you specify "auto" it means - parent: CPU count, child: 0
///   - delay: set delay in msecs for "run"
///   - work_queue_name: queue where child should be registered automatically
///   - work_queue_addr: 
///     - performs queue.add_listener_at(exec_at_addr, queue_name)
///   - reg_addr: node where child should register after start
///   - reg_name: source name at that node
///     - performs core.reg_node_at(exec_at_addr, source_name)
///
///   Select parent or child and create appropriate process.
/// 
/// Watchdog parent task phases:
/// - init: 
///   a) kill all existing children omiting current process
///   b) change status to "starting"
/// - run (status = "starting")
///   a) if child count < required then start one child process, otherwise change status to "running"  
/// - run - every 30 secs:  
///   a) find unreachable children, for each found - perform close, kill
///   b) calculate number of missing children, start child(-ren) if > 0
/// - requestStop:
///   a) send "close" to each child, change status to "stopping"
/// - run (status = "stopping"):
///   a) if no child found - change status to "stopped" and close task
///   b) if any child found - check last status time, if timeout - kill
///
/// Watchdog child task phases:
/// - run - every 30 secs:
///   a) if parent process not reachable - exit app
///   
// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// sc
#include "sc/proc/LimitSingleInstance.h"

// grd
#include "grd/core.h"
#include "grd/TaskImpl.h"
#include "grd/ModuleImpl.h"


// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const unsigned int SC_DEF_WATCHDOG_DELAY = 30000; ///(ms)
const int SC_WATCHDOG_KILL_RETRIES = 1000;
const unsigned int SC_WATCHDOG_KILL_TIMEOUT = 10000; // ms
const unsigned int SC_WATCHDOG_MSG_TIMEOUT = 1000*60; // ms
const unsigned int SC_WATCHDOG_START_CHILD_DELAY = 100; // ms

static const char *SC_WATCHDOG_PROTOCOL = "wxcs";
static const char *SC_WATCHDOG_MUTEX_NAME = "Global\\{1XHN66z7tfmMx44SrNdhIBKQhRiZXFVrgGgu}";
static const char *SC_WATCHDOG_INIT_CMD_MACRO = "#init_commands#";
static const char *SC_WATCHDOG_CHILD_ID_PATTERN = "#_CID#";

//#define SC_WATCHDOG_CHILDREN_MINIMIZED 1
#define SC_WATCHDOG_CHILDREN_LOWPRIOR 1

#ifdef SC_LOG_ENABLED
#define SC_LOG_WATCHDOG
#endif

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scW32WatchdogParentTask
// ----------------------------------------------------------------------------
/// task for managing children for parent process
class scW32WatchdogParentTask: public scTask {
public:
  scW32WatchdogParentTask(
    const scString &childExecPath, 
    const scString &childExecParams = "",
    unsigned int startChildDelay = 0, 
    uint aliveTimeout = 0,
    uint aliveInterval = 0, 
    unsigned int childCount = 0,
    scLimitSingleInstance *limitInstance = SC_NULL,
    bool childTalk = true,
    const scString &workQueueAddr = "",
    const scString &workQueueName = "",
    const scString &regAddr = "",
    const scString &regName = "",
    const scString &childAddr = ""
    );
  virtual ~scW32WatchdogParentTask();  
  void postEchoToChild(ulong64 pid);
protected:  
  virtual void intInit();  
  virtual void intDispose(); 
  virtual int intRun();  
  void closeChildren();
  void startChildren(unsigned int a_count);
  void terminateChildren();
  void startChildCollection(int count, const scString &cmd, const scString &params);
  bool checkChildCount();
  void sendMsgToEachChild();
  void calcChildAddress(ulong64 pid, scMessageAddress &output);
  void calcExecParamsForChild(scString &output);
protected:
  unsigned int m_startChildDelay;
  uint m_aliveTimeout;
  uint m_aliveInterval;  
  unsigned int m_requiredChildCount;  
  scString m_childExecPath;
  scString m_childExecParams;
  scLimitSingleInstance *m_singleInstanceObj;
  bool m_childTalk;
  scString m_workQueueAddr;
  scString m_workQueueName;
  scString m_regAddr;
  scString m_regName;
  scString m_childAddr;
};

// ----------------------------------------------------------------------------
// scW32WatchdogChildTask
// ----------------------------------------------------------------------------
/// task for managing child process
class scW32WatchdogChildTask: public scTask {
public:
  scW32WatchdogChildTask(unsigned int delay = 0);
  virtual ~scW32WatchdogChildTask();  
protected:
  virtual int intRun();  
  void checkParent();
protected:
  unsigned int m_delay;  
  unsigned long m_parentPid;
};

// ----------------------------------------------------------------------------
// scW32WatchdogModule
// ----------------------------------------------------------------------------
/// module creates appropriate watchdog task
class scW32WatchdogModule: public scModule {
public:
  // -- creation --
  scW32WatchdogModule();
  virtual ~scW32WatchdogModule();  
  // -- module support --
  virtual scStringList supportedInterfaces() const;
  virtual int handleMessage(scMessage *message, scResponse &response);
  virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
protected:  
  // --- commands ---
  int handleCmdInit(scMessage *message, scResponse &response);
  // --- other ---
  scTask *prepareParentTask(scMessage *message, scLimitSingleInstance *limitInstance);
  scTask *prepareChildTask(scMessage *message);
  void readParamsForParentTask(
    scMessage *message,
    scDataNode &outParams); 
//    scString &childExecPath, scString &childExecParams,
//    unsigned int &delay, unsigned int &childCount, bool &childTalk);
  void readParamsForChildTask(
    scMessage *message, unsigned int &delay);
};

#endif // _scW32WatchdogModule_H__