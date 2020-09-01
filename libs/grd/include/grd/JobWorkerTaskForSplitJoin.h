/////////////////////////////////////////////////////////////////////////////
// Name:        scJobWorkerTaskForSplitJoin.h
// Project:     scLib
// Purpose:     Persistent job task that splits work into several chunks.
//              Handles restarts and works in parallel.
// Author:      Piotr Likus
// Modified by:
// Created:     01/06/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _JOBWORKERTASKFORSPLITJOIN_H__
#define _JOBWORKERTASKFORSPLITJOIN_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file scJobWorkerTaskForSplitJoin.h
///
/// Algorithm:
/// - read config
/// - read state
/// - if state empty: init population
/// - otherwise: fill population with fitness from state
/// - is evaluation required - continue it
///   - prepare message pack
///   - post it
///   - on results received: commit
/// - if end of evaluation: 
///   - run world evolution
///   - log statistics
///   - on end of experiment: stop job 

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/core.h"
#include "grd/JobWorkerTask.h"
#include "grd/JobWorkFile.h"

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
class scJobWorkerTaskForSplitJoin: public scJobWorkerTask {
public:
  scJobWorkerTaskForSplitJoin(scMessage *message);
  virtual ~scJobWorkerTaskForSplitJoin() {};
  virtual void handleWorkerResult(uint chunkId, const scDataNode &result) = 0;
  virtual void handleMessagePackProcessed();
  virtual void handleMessagePackErrors(scMessagePack *pack);
  virtual int runStep();
protected:  
  virtual void intStartWork();
  void postNotifyCmd(const scString &notifyAddr, const scString &notifyCmd);  
  void postMessagePack(uint &postedCount, const scString &workQueue, 
    uint chunkCount, const scDataNode &config);
  void readConfig(const scString &path, scDataNode &output);
  virtual bool isEndOfWork() = 0;
  virtual bool isEndOfWorkUnit() = 0;
  virtual void afterSyncAction(int action, const scDataNode &context);
  virtual void performAfterLoadState();
  virtual void performAfterCommit();
  void postNextPack(bool sync);
  void postMessagePack(uint &postedCount);
  void readConfig(const scDataNode &jobParams);
  virtual void handleStateLoaded();
  scMessagePack *newMessagePack();
  virtual void fillMessagePack(scMessagePack &pack) = 0;
  virtual void initProcessing();
  virtual void resumeProcessing();
  virtual void processWorkUnit();
  virtual void loadWorkUnitState() = 0;
  virtual void saveWorkUnitState() = 0;
  virtual void handleWorkUnitReady();
protected:
  scDataNode m_config; 
  scString m_workQueueName; 
  uint m_chunksPerCommit;
  uint m_chunkOffset;
  uint m_nextChunkOffset;
  bool m_restarted;
};

#endif // _JOBWORKERTASKFORSPLITJOIN_H__