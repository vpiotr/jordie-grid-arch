/////////////////////////////////////////////////////////////////////////////
// Name:        JobCommon.h
// Project:     scLib
// Purpose:     Common classes and definitions for persistent job support.
// Author:      Piotr Likus
// Modified by:
// Created:     02/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _JOBCOMMON_H__
#define _JOBCOMMON_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file JobCommon.h
///
/// 

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// #include ".."

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const int JMM_DEF_PRIORITY = 5;

const ulong64 JQ_DEF_PURGE_INTERVAL = ulong64(65)*(60*60*24*1000); // 65 days
const ulong64 JQ_DEF_PURGE_CHECK_INTERVAL = ulong64(20)*(60*1000); // 20 mins

const ulong64 JDEF_STEP_TIMESLICE = ulong64(200); // 200 ms (5 per sec)

// log message codes
const int JMC_SYSTEM_BASE = 0;
const int JMC_USER_BASE = 1000;

const int JMC_OTHER       = JMC_SYSTEM_BASE + 1;

const int JMC_JOB_START   = JMC_SYSTEM_BASE + 10;
const int JMC_JOB_END     = JMC_SYSTEM_BASE + 11;
const int JMC_JOB_END_ERROR = JMC_SYSTEM_BASE + 12;

const int JMC_SUB_UNKNOWN = JMC_SYSTEM_BASE + 20;
const int JMC_SUB_ERROR   = JMC_SYSTEM_BASE + 21;

const int JMC_JOB_COMMIT_OK  = JMC_SYSTEM_BASE + 30;
const int JMC_JOB_ROLLBACK_OK = JMC_SYSTEM_BASE + 31;
const int JMC_JOB_RESTART_OK = JMC_SYSTEM_BASE + 32;
const int JMC_JOB_RETURN_OK = JMC_SYSTEM_BASE + 33;
const int JMC_JOB_ABORT_OK = JMC_SYSTEM_BASE + 34;
const int JMC_JOB_STOP_OK = JMC_SYSTEM_BASE + 35;

const int JMC_JOB_COMMIT_ERR  = JMC_SYSTEM_BASE + 40;
const int JMC_JOB_ROLLBACK_ERR = JMC_SYSTEM_BASE + 41;
const int JMC_JOB_RESTART_ERR = JMC_SYSTEM_BASE + 42;
const int JMC_JOB_RETURN_ERR = JMC_SYSTEM_BASE + 43;
const int JMC_JOB_ABORT_ERR = JMC_SYSTEM_BASE + 44;
const int JMC_JOB_STOP_ERR = JMC_SYSTEM_BASE + 45;

const int JMC_JOB_TIMEOUT = JMC_SYSTEM_BASE + 50;
const int JMC_TRANS_TIMEOUT = JMC_SYSTEM_BASE + 51;

// job end codes
const int JEC_OK = 0;
const int JEC_TRANS_ERROR = 1; // transaction error
const int JEC_WU_EXCEPTION = 2; // exception during work unit or sync execution
const int JEC_WU_OTHER_ERROR = 3; // other error

//database & value limits
const int JOB_MAX_STATE_VAR_LEN = 1024;


enum scJobMessageLevel {
  jmlError = 1,
  jmlWarning = 2,
  jmlInfo = 4,
  jmlAll = 7
};

enum scJobLogType {  
  jltError = 1,
  jltWarning = 2,
  jltInfo = 4,
  jltMax
};

enum scJobStatus {
  jsNull = 0,   // unset
  jsPaused,     // waiting for resume
  jsWaiting,    // waiting for conditions
  jsReady,      // ready to be executed 
  jsSubmitted,  // added to queue
  jsRunning,    // running on some node  
  jsEnded,      // ended successfully 
//  jsKilled,     // killed by user
  jsPurged,     // results removed (archived). to be deleted from history
  jsAborted,    // error occured
  jsSleep,      // job is sleeping, waiting for queue open
};

static const char *JMM_STATUS_NAMES = "null,paused,waiting,ready,submited,running,ended,purged,aborted,sleep";
 
static scString JMM_RESTYP_TEMPFILE = "TMPF"; // temporary file, delete on commit and rollback
static scString JMM_RESTYP_WORKFILE = "WRKF"; // work file, delete on rollback 
static scString JMM_RESTYP_OBSOLFILE = "OBSF"; // obsolete file, delete on commit

static const char *JMM_NO_MATCHES_TEXT = "--> No matches found!";

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------


#endif // _JOBCOMMON_H__