/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueWorker.h
// Project:     grdLib
// Purpose:     Worker part of work queue handling.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _WORKQUEUEWORKER_H__
#define _WORKQUEUEWORKER_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
///- worker with notifier
///  - S: prepare all server part
///  - A: prepare worker as object
///  - A: add worker to server: (registerWorker)
///    - S: read handled msgs
///    - S: connect it to notifier for selected msgs
///  - S: use "worker" module - sends all msgs to notifier
///       (some will be handled)
///  - W: worker - after x msec - send "yield"
  
// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "sc/events/Events.h"

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
class scWqWorker {
public:
  scWqWorker() {}
  virtual ~scWqWorker() {}
  virtual void getSupportedCommands(scStringList &output) const = 0;
  virtual bool process(const scString &command, const scDataNode &args, scDataNode &output) = 0;
  virtual void yield() = 0;
};

class scWqModalWorker: public scWqWorker {
public:
  scWqModalWorker(): m_ysignal(SC_NULL), scWqWorker() {}
  virtual ~scWqModalWorker() {};
  void setYieldSignal(scSignal *ysignal) { m_ysignal = ysignal; }
  virtual void yield() { if (m_ysignal != SC_NULL) m_ysignal->execute(); }
protected:  
  // add process ID as worker_id
  void addWorkerIdToResult(scDataNode &output);
/* Moved to WorkerStatsTool:
  void resetTimers(const scDataNode &statFilterList, scDataNode &statBackup);
  void resetCounters(const scDataNode &statFilterList, scDataNode &statBackup);
  void prepareTimerStats(const scDataNode &statFilterList, scDataNode &output);
  void prepareCounterStats(const scDataNode &statFilterList, scDataNode &output);
  void restoreTimerStats(const scDataNode &values);
  void restoreCounterStats(const scDataNode &values);
  void removeEmptyStats(scDataNode &output);
*/
protected:
  scSignal *m_ysignal;  
};

class scWqWorkerListener: public scListener {
public:
// construct
  scWqWorkerListener(scWqWorker *target): m_target(target), scListener() {}
  virtual ~scWqWorkerListener() {}
// run
  virtual bool process(const scString &eventName, const scDataNode *aInput, scDataNode *aOutput) { 
    if (aInput != SC_NULL)
      return m_target->process(eventName, *aInput, *aOutput);    
    else { 
      scDataNode nullData;
      return m_target->process(eventName, nullData, *aOutput);    
    }  
  }
protected:
  scWqWorker *m_target;  
};

#endif // _WORKQUEUEWORKER_H__