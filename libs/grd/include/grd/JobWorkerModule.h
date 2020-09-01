/////////////////////////////////////////////////////////////////////////////
// Name:        JobWorkerModule.h
// Project:     scLib
// Purpose:     Module supporting messages for worker node performing jobs.
// Author:      Piotr Likus
// Modified by:
// Created:     01/12/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _JOBWORKERMODULE_H__
#define _JOBWORKERMODULE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file JobWorkerModule.h
///
/// Module supports the following messages:
/// * job_worker.start_work 
///     parameters: job_id,lock_id,trans_id,command,log_level,msg_level,priority,job_params
///   - when received, extracts "command" and forwards this command to same node 
//    - this should create a new class derived from JobWorkerTask which performs all job support
///     - commitWork
///     - rollbackWork
///     - endWork (success: bool) 
///   - if command not recognized, result should be returned with error
// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/core.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
typedef std::list<scTask *> scJobTaskList;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scJobWorkerModule: public scModule {
public:
  scJobWorkerModule();
  virtual ~scJobWorkerModule();
  virtual int handleMessage(scMessage *message, scResponse &response);
  virtual scStringList supportedInterfaces() const;
  void handleSubmitError(ulong64 jobId, uint lockId, const scString &returnAddr, const scString &errorMsg);
  void handleSubmitSuccess(ulong64 jobId, uint lockId, const scString &returnAddr);
protected:  
  int handleCmdStartWork(scMessage *message, scResponse &response);
  int handleCmdCancelWork(scMessage *message, scResponse &response);
  int forwardMessage(const scString &command, const scDataNode &params, scResponse &response);
  bool cancelWork(ulong64 jobId);
  scTask *findJobTaskForMessage(ulong64 jobId, const scString &message);
protected:  
  scJobTaskList m_jobTasks;
};

#endif // _JOBWORKERMODULE_H__