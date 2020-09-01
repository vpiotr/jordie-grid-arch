/////////////////////////////////////////////////////////////////////////////
// Name:        JobManagerModule.h
// Project:     scLib
// Purpose:     Persistent job management classes.
// Author:      Piotr Likus
// Modified by:
// Created:     14/12/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _SCJOBMANAGERMODULE_H__
#define _SCJOBMANAGERMODULE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file Job.h
///
/// Handles persistent jobs. These jobs can be restarted after power failure.
/// Jobs can be distributed among many nodes or moved between nodes.
/// Job information is stored in SQL database. Special interface class is used
/// for this purpose.
/// 
/// Supported commands:
///     +* job.init_manager <dbpath> - initialize job manager module
///     +* job.define name=<name>[,command=<command>,base=<base_def>,<_system_params>,<job_params>] 
///       - each param is named
///     +* job.change_def name=<name>,<named_param_value_list>
///     +* job.remove_def name=<name> - remove job definition
///     +* job.list_defs [filter='text*?'] - list job definitions
///     +* job.desc_def name=<name> - describe job definition
/// 
/// Job queue management
/// 
///     +* job.start_queue name=<queue_name>, target=squeue_addr, return=return_addr (loads queue jobs, remove/inc locks and distributes jobs)
///     +* job.stop_queue <queue_name> (stop jobs, remove/inc locks)
///     +* job.list_queues - list all queues running on node (names)
///     +* job.list_jobs name=<queue_name> - list all jobs on queue (name,status,worker addr)
///     * job.set_queue_root <queue_name>,<path> - set root dir for jobs from queue (empty - any), used for delete
/// 
/// Job instance management:
/// 
///     +* job.start name=<def_name>,queue=<queue_name>,<system_params>,<job_params>,<start_paused:bool> - add to "active jobs" queue with status "active" or "waiting"
///     +* job.ended <job_id>, <lock_id> - sent from worker, job ended normally, perform commit, change status to "ended"
///     +* job.restart <jobid> - start from the beginning (purge all work, inc lock, activate, any status)
///     +* job.return <jobid> - abort processing on work node and return job to queue with ready state, perform rollback
///     +* job.stop <jobid> - stop immediatelly with rollback
///     +* job.purge <jobid> - only if ended,aborted - delete all job info: output files, values, job info
///     * job.pause <wait=true|false> - temporary stop working on job, mode: roll-back to last commit point or wait for next commit point
///     * job.resume <jobid> - start from last exec point on any worker (inc lock, required status = paused)
/// 
/// State management
///     +* job.get_state <job_id>
///     +* job.set_vars <job_id>, <trans_id>, vars=<list of var names>
///     * job.disp_vars <job_id>
///     +* job.alloc_res job_id, trans_id, name=<logic_name>,type="file",path=<path>
///     +* job.dealloc_res job_id, trans_id, name=<logic_name>
///     * job.wait_for_res <list_of_resources> - wait until all the resources are freed using Dealloc
/// 
/// Transaction management
///     +* job.commit job_id, trans_id, chained: bool - if chained, on success returns new trans_id value
///     +* job.rollback job_id, trans_id, chained: bool - if chained, on success returns new trans_id value 
/// 
/// Job log support
/// 
///     +* job.log_text severity=<value>,text=<message>,code=<msg_code>
///     - use task.logText
/// 
/// Job console support
/// 
///     - use task.displayText
/// 
/// Job system parameters
/// 
///     - trans_sup: transaction support (true/false)
///     - job_timeout - maximum time of processing (ms)
///     - commit_timeout - maximum time of single work unit (between commits) (ms)
///     - log_level - level of messages to be logged (bits 0-2, "all" = 7)
///     - msg_level - level of messages to be displayed (bits 0-2, "all" = 7)
///     - retry_limit - how many times job can be automatically restarted on job error
///     - msg_addr - address when display messages should be posted, empty = submitter
///     - log_addr - address when log messages should be posted, empty = submitter
/// 
/// Job application-defined standard parameters
/// 
///     - chunk_count - total number of chunks for the whole job
///     - chunks_per_commit - number of chunks between commits
///     - commit_interval - maximum time between commits
///     - step_size - maximum number of iterations per step in job task 

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/DbBase.h"

#include "grd/core.h"
#include "grd/JobQueueTask.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
static const char *JMM_DEFAULT_DEF_NAME = "_default";
static const char *JMM_JQMAN_PFX = "jq";

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scJobManagerModule 
// ----------------------------------------------------------------------------
/// This module should be created / registered only in queue manager process.
class scJobManagerModule: public scModule 
{
public:
    scJobManagerModule();
    virtual ~scJobManagerModule();
    virtual void init();
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
    virtual scStringList supportedInterfaces() const;
    scDbBase *getDatabase();
    void setDbPath(const scString &path);
    scString getDbPath();
    void setSafeRootList(const scString &value);
    scString getSafeRootList();
    
protected:
//--- job definitions
    int handleCmdInitManager(scMessage *message, scResponse &response);
    int handleCmdDefine(scMessage *message, scResponse &response);
    int handleCmdChangeDef(scMessage *message, scResponse &response);
    int handleCmdRemoveDef(scMessage *message, scResponse &response);
    int handleCmdListDefs(scMessage *message, scResponse &response);
    int handleCmdDescDef(scMessage *message, scResponse &response);
//--- queue management    
    int handleCmdStartQueue(scMessage *message, scResponse &response);
    int handleCmdStopQueue(scMessage *message, scResponse &response);
    int handleCmdListQueues(scMessage *message, scResponse &response);
//--- job management    
    int handleCmdStartJob(scMessage *message, scResponse &response);    
    int handleCmdListJobs(scMessage *message, scResponse &response);
    int handleCmdJobEnded(scMessage *message, scResponse &response);    
    int forwardQueueMsg(scMessage *message, scResponse &response);
protected:    
    void initDatabase();
    void prepareDbStructure();    
    bool defineJob(const scDataNode &params);
    bool intDefineJob(const scString &name, const scString &baseName, const scString &command, 
      const scDataNode &sysParams, const scDataNode &jobParams);
    bool removeJobDef(const scString &name);
    void processDefParams(const scDataNode &input, scDataNode &output);
    bool changeJobDef(scDataNode &changeParams);
    ulong64 getJobDefId(const scString &name);
    scString prepareSqlPatternFor(const scString &filter);
    void listJobDefs(const scString &filter);
    bool descJobDef(const scString &name);
  // --- queue management ---  
    bool queueExists(const scString &name);
    scJobQueueTask *startQueue(const scString &name, const scString &targetAddr, const scString &returnAddr,
      ulong64 purgeInterval, ulong64 purgeCheckInterval);
    scJobQueueTask *findQueue(const scString &name);
    scJobQueueTask *checkQueue(const scString &name);
    void closeQueue(const scString &name);
    void setUnknownQueueError(const scString &qname, scResponse &response);
    scTask *prepareManager(scMessage *message);    
    void listQueues(const scString &filter);
    void listJobs(const scString &filter);
    void closeAllQueues();
    scString getQueueForJobId(ulong64 jobId);
    int forwardQueueMsgByJobId(scMessage *message, scResponse &response);
protected:
    scDbBase *m_db;    
    scString m_dbPath;
    scString m_safeRootList;
private:
  scJobQueueTaskList m_managers;
};


// ----------------------------------------------------------------------------
// scJobManagerModule 
// ----------------------------------------------------------------------------
/// This module should be created / registered in worker process.
/*
class JobWorkerModule: public scModule 
{
public:
    JobWorkerModule();
    virtual ~JobWorkerModule();
    virtual void init();
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scStringList supportedInterfaces() const;
};
*/

#endif // _SCJOBMANAGERMODULE_H__