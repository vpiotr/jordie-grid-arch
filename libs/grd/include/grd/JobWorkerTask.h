/////////////////////////////////////////////////////////////////////////////
// Name:        JobWorkerTask.h
// Project:     scLib
// Purpose:     Base class for worker tasks.
// Author:      Piotr Likus
// Modified by:
// Created:     02/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _JOBWORKERTASK_H__
#define _JOBWORKERTASK_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file JobWorkerTask.h
/// 
/// JobWorkerTask is a task performing "job" execution.
/// It can be created by any module in response to command stored in 
/// "job_worker.start_work" message.
/// 
/// When the job is finished this task should send:
/// * "job.ended" <job_id>, <lock_id>
/// message to node addressed by "return address" parameter.
/// 
/// Task supports transaction processing with the following messages sent
/// to "return address" node:
/// * job.commit_work <trans_id> 
/// * job.rollback_work <trans_id>
/// * job.reg_temp_file <trans_id>, <path>
/// * job.unreg_temp_file <trans_id>, <path>
/// * job.get_state <job_id>, trans_id=0|<trans_id>, param_name=<param_name_filter> 
/// * job.set_state <job_id>, trans_id=0|<trans_id>, list:[param_name=<name>, param_type=param|file, value=<value>]
/// 

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"

#include "grd/core.h"
#include "grd/JobCommon.h"
#include "grd/MessagePack.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
typedef std::auto_ptr<scMessagePack> scMessagePackGuard;
typedef boost::intrusive_ptr<scMessagePack> scMessagePackTransporter;

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
// synchro operations
static const int jwtsoSysBase = 0;
static const int jwtsoUsrBase = 1000;

static const int jwtsoNull           = 0;
static const int jwtsoSyncPoint      = jwtsoSysBase + 1;
static const int jwtsoCommit         = jwtsoSysBase + 2;
static const int jwtsoRollback       = jwtsoSysBase + 3;
//static const int jwtsoGetVars        = jwtsoSysBase + 3;
//static const int jwtsoSetVars        = jwtsoSysBase + 4;
static const int jwtsoAddTempFile    = jwtsoSysBase + 4;
static const int jwtsoRemoveTempFile = jwtsoSysBase + 5;
static const int jwtsoEndWork        = jwtsoSysBase + 6;
static const int jwtsoPost           = jwtsoSysBase + 7;
static const int jwtsoLoadState      = jwtsoSysBase + 8;

enum jwtSyncStage {
  jwtssNone,
  jwtssBefore,
  jwtssExec,
  jwtssAfter
};

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scJobWorkerTask: public scTask {
public:
  scJobWorkerTask(scMessage *message);
  virtual ~scJobWorkerTask();
  virtual bool needsRun();
  bool acceptsMessage(const scString &command, const scDataNode &params);
protected:
  // task framework - protected
  virtual int runStarting(); 
protected:
  // job framework
  void startWork();    
  void endWork(int status = 0, const scString &msg = scString(""));    
  virtual void processJobParams(const scDataNode &params);
  virtual void intStartWork();    
  virtual bool inTransaction();
  virtual void commitWork(bool chained = true);    
  virtual void rollbackWork(bool chained = true);    
  /// returns >1 if there is a need for more iterations
  virtual int runStep();
  virtual void intEndWork(int status, const scString &msg);    
  // variables
  virtual void setVars(const scDataNode &vars);
  virtual void setVar(const scString &name, const scDataNode &value);
  virtual void setVar(const scString &name, const scString &value);
  virtual void setVarAsLongText(const scString &name, const scString &value);
  virtual void getVars(scDataNode &output);
  virtual void getVar(const scString &name, const scString &defValue, scString &output);
  virtual scString getVar(const scString &name, const scString &defValue = scString(""));
  virtual void getVar(const scString &name, const scDataNode &defValue, scDataNode &output);
  virtual scDataNode getVar(const scString &name, const scDataNode &defValue);
  virtual void getVarAsLongText(const scString &name, const scString &defValue, scString &output);
  virtual scString getVarAsLongText(const scString &name, const scString &defValue = scString(""));
  virtual bool stateRequired();
  virtual void allocResource(const scString &name, const scString &resType, const scString &resPath);
  /// list of values: name, type, path
  virtual void allocResource(const scDataNode &resList);
  virtual void deallocResource(const scString &name);
  /// list of values: name
  virtual void deallocResource(const scDataNode &resList);
  void doCommitWork(const scDataNode &context);
  void doRollbackWork(const scDataNode &context);
  void doAfterTransWork(const scDataNode &context);
  virtual bool handleTransActionFailed(const scString &command);
  bool handleWorkUnitError(std::runtime_error &excp);
  scDataNode &getJobParams();
  scString getReturnAddr();
  virtual void logText(const scString &a_text, scJobLogType severity = jltInfo, uint msgCode = 0);
  void postJobMessage(const scString &command, const scDataNode &params, const scString &address = scString(""));
  virtual void displayText(const scString &a_text, scJobLogType severity = jltInfo);
  ulong64 getJobId() { return m_jobId; }
protected:  
  // --> synchronization
  void postWithSync(scEnvelope *envelope);
  void syncPoint();
  void syncAction(int action, const scDataNode *context, bool syncBefore, bool syncAfter);
  bool isAllSynced();
  bool inSyncAction();
  virtual void beforeSyncAction(int action, const scDataNode &context);
  virtual void execSyncAction(int action, const scDataNode &context);
  virtual void afterSyncAction(int action, const scDataNode &context);
  void resetSyncActions();  
  // <-- synchronization
protected:
  // --> job support
  bool m_started; 
  bool m_ended;
  bool m_stateLoaded; 
  scString m_returnAddr;
  scDataNode m_jobParams;
  ulong64 m_jobId;
  uint m_lockId;
  ulong64 m_transId;
  uint m_logLevel;
  uint m_msgLevel;
  scString m_logAddr;
  scString m_msgAddr;
  // --> synchronization
  scMessagePackTransporter m_outputSyncPack;
  bool m_syncBefore;
  bool m_syncAfter;
  jwtSyncStage m_syncStage;
  scDataNode m_syncContext;
  int m_syncAction;
  // <-- synchronization
private:
  virtual int intRun();   
  void postWorkEnded(int status, const scString &msg);
  void doEndWork(const scDataNode &context);
  bool checkSyncAction();
  void processTaskParams(const scDataNode &params);
  void doLoadState(const scDataNode &context);
  virtual void loadState();
  void setVarsLocal(const scDataNode &vars);
  void setVarsRemote(const scDataNode &vars);
  scDataNode m_stateVars;
};


#endif // _JOBWORKERTASK_H__