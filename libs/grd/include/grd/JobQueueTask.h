/////////////////////////////////////////////////////////////////////////////
// Name:        JobQueueTask.h
// Project:     scLib
// Purpose:     Persistent job queue - management task.
// Author:      Piotr Likus
// Modified by:
// Created:     29/12/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _SCJOBQUEUETASK_H__
#define _SCJOBQUEUETASK_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file JobQueueTask.h
///
/// Implements task responsible for running jobs in queue. 

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/DbBase.h"

#include "grd/core.h"
#include "grd/JobCommon.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
static const char *JMM_JDEF_CLASS_SYS = "sys";
static const char *JMM_JDEF_CLASS_JOB = "job";
const ulong64 JQT_DEF_TIMEOUT_CHECK_DELAY = 10*1000; // msec

#define JQT_LOG_ENABLED 

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scJobQueueTask;
typedef std::list<scJobQueueTask *> scJobQueueTaskList;

class scJobQueueTask: public scTask {
public:
  scJobQueueTask(const scString &queueName, const scString &targetAddr, const scString &returnAddr, scDbBase *db, 
    const scString &safeRootList, ulong64 purgeInterval, ulong64 purgeCheckInterval);
  virtual ~scJobQueueTask();
  virtual bool needsRun();
  virtual void openQueue();
  virtual void closeQueue();
  virtual int handleMessage(scEnvelope &envelope, scResponse &response);
  scString getQueueName();
  scString getTargetAddr();
  scString getReturnAddr();
  int handleQueueMsg(scMessage *message, scResponse &response);
  void handleSubmitSuccess(ulong64 jobId, const scString &workerAddr, uint lockId);
  void handleSubmitError(ulong64 jobId, const scString &errorMsg, uint lockId);
  virtual int runStopping(); 
protected:
  // --- other ---  
  void prepareJobStartParams(ulong64 jobDefId, const scDataNode &inParams, scDataNode &outCoreParams, scDataNode &outJobParams);
  void getJobDefInfo(ulong64 jobDefId, scDataNode &coreParams, scDataNode &jobParams);  
  ulong64 getJobDefId(const scString &name);
  void insertJobToDb(ulong64 jobDefId, const scString &command, uint initStatus, uint logLevel, uint msgLevel, 
    uint priority, uint jobTimeout, uint commitTimeout, uint retryLeft, bool transSup, const scDataNode &jobParams, ulong64 &jobId, uint &lockId);
  void insertJobToJobQueue(ulong64 jobId, const scString &command, uint initStatus, uint lockId, uint logLevel, 
    uint msgLevel, uint priority, uint jobTimeout, uint commitTimeout, uint retryLeft, bool transSup, const scDataNode &jobParams, 
    const scString &workerAddr, scDateTime jobStartDt, scDateTime transStartDt);
  bool startJob(const scString &defName, const scDataNode &params);
  void checkActivateJob(ulong64 jobId, bool awake = false);
  void activateJob(ulong64 jobId);
  void addJobLogEntry(ulong64 jobId, scJobLogType a_type, int messageCode, const scString &message);
  void changeJobStatus(ulong64 jobId, scJobStatus newStatus, long64 lockId = -1);
  scString getTimestamp();
  scDateTime getCurrentDateTimeFromDb();
  bool changeJobWorker(ulong64 jobId, const scString &workerAddr, uint lockId);
  bool handleJobEnded(ulong64 jobId, uint lockId, ulong64 transId, bool resultOK, const scString &errorMsg);
  bool jobExists(ulong64 jobId, long64 lockId = -1);
  void loadJobsFromDb();
  void getJobParams(ulong64 jobId, scDataNode &jobParams);
  void insertJobToJobQueue(const scDataNode &jobData, const scDataNode &jobParams);
  void checkActivateOnAllJobs();
  virtual void intInit(); 
  void clearJobQueue();
  void returnAllSubmittedInDb();
  void emptyTarget();    
  bool startTrans(ulong64 jobId, ulong64 &transId);
  bool commitJob(ulong64 jobId, uint lockId, ulong64 transId);
  bool rollbackJob(ulong64 jobId, uint lockId, ulong64 transId, bool ignoreLock = false);
  bool removeJobVars(ulong64 jobId, ulong64 transId);
  bool removeAllocations(ulong64 jobId);
  bool removeAllocations(ulong64 jobId, ulong64 transId, bool asCommit, bool allTypes = false);
  bool rollbackAllJobTrans(ulong64 jobId);
  bool removeAllocationsFromDb(ulong64 jobId, ulong64 transId, const scString &resType);
  bool removeTempFileByPath(const scString &a_path);
  bool canRemoveFile(const scString &a_path);
  bool restartJob(ulong64 jobId);
  bool returnJob(ulong64 jobId);
  bool abortJob(ulong64 jobId);  
  bool purgeJob(ulong64 jobId);
  scDateTime getJobExecTime(ulong64 jobId);
  bool removeJobTrans(ulong64 jobId);
  bool clearJobLog(ulong64 jobId);  
  bool isTimeForPurgeCheck();
  void checkPurgeCheckNeeded();
  void runGlobalPurgeCheck();
private:
  virtual int intRun();
  bool getJobState(ulong64 jobId, scResponse &response);
  bool getJobStateData(ulong64 jobId, scDataNode &output);
  bool setVars(ulong64 jobId, uint lockId, ulong64 transId, const scDataNode &vars);
  bool allocJobRes(const scDataNode &head, const scDataNode &item);
  bool allocJobRes(ulong64 jobId, uint lockId, ulong64 transId, const scString &name, const scString &path, const scString &resType);
  bool deallocJobRes(const scDataNode &head, const scDataNode &item);
  bool deallocJobRes(ulong64 jobId, uint lockId, ulong64 transId, const scString &name);
  bool commitJobCopyVarsToBase(ulong64 jobId, ulong64 transId);
  bool rollbackJobRemoveVars(ulong64 jobId, ulong64 transId);
  bool removeResAllocations(ulong64 jobId, ulong64 transId, const scString &resType);
  bool closeTrans(ulong64 jobId, ulong64 transId);  
  bool isCurrentJobLock(ulong64 jobId, uint lockId);
  uint getCurrentJobLock(ulong64 jobId);
  bool increaseJobLock(ulong64 jobId);
  void cancelJobOnWorker(ulong64 jobId);
  void postCancelJobOn(ulong64 jobId, const scString &addr);
  bool prepareJobInQueue(ulong64 jobId);
  bool loadOneJobFromDb(ulong64 jobId);
  bool removeJobVars(ulong64 jobId);  
  uint getJobStatus(ulong64 jobId);
  void addJobOperStatus(ulong64 jobId, const scString &operName, bool success, uint msgCodeOK, uint msgCodeErr);
  void changeRunningToSleepingInDb();
  scString getLatestTransStartDt(ulong64 jobId);
  bool isJobInQueue(ulong64 jobId);
  bool setTransStartDt(ulong64 jobId, const scString &value);
  bool clearTransStartDt(ulong64 jobId);  
  void checkTimeoutsNeeded();
  bool isTimeForTimeoutCheck();
  void runTimeoutCheck();  
  void checkTimeoutsForJob(ulong64 jobId, bool transSup, uint jobTimeout, uint transTimeout, 
    scDateTime jobStartDt, scDateTime transStartDt, scDateTime nowDt);
  void setRetryCount(ulong64 jobId, uint value);
  void handleTransTimeout(ulong64 jobId); 
  void handleJobTimeout(ulong64 jobId);
  void purgeJobFull(ulong64 jobId);  
  void removeJob(ulong64 jobId);
  bool stopJob(ulong64 jobId);
  ulong64 getJobIdParam(const scDataNode &params);  
  // --- commands ---
  int handleCmdStartJob(scMessage *message, scResponse &response);
  int handleCmdStopJob(scMessage *message, scResponse &response);
  int handleCmdJobEnded(scMessage *message, scResponse &response);
  int handleCmdGetState(scMessage *message, scResponse &response);
  int handleCmdSetVars(scMessage *message, scResponse &response);
  int handleCmdAllocRes(scMessage *message, scResponse &response);
  int handleCmdDeallocRes(scMessage *message, scResponse &response);
  int handleCmdCommit(scMessage *message, scResponse &response);
  int handleCmdRollback(scMessage *message, scResponse &response);
  int handleCmdRestartJob(scMessage *message, scResponse &response);
  int handleCmdReturnJob(scMessage *message, scResponse &response);
  int handleCmdPurgeJob(scMessage *message, scResponse &response);
  int handleCmdDispVars(scMessage *message, scResponse &response);
  int handleCmdLogText(scMessage *message, scResponse &response);
protected:
  scString m_queueName; 
  scString m_targetAddr;
  scString m_returnAddr; 
  scDbBase *m_db;
  scDataNode m_jobList;
  scDataNode m_safeRootList;
  bool m_closing;
  cpu_ticks m_lastTimeoutCheck;
  cpu_ticks m_timeoutDelay;
  cpu_ticks m_purgeInteval;
  cpu_ticks m_purgeCheckInterval;  
  cpu_ticks m_lastPurgeCheck;  
};

#endif // _SCJOBQUEUETASK_H__