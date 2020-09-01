/////////////////////////////////////////////////////////////////////////////
// Name:        scJobWorkerTaskForSplitJoin.cpp
// Project:     metaGp
// Purpose:     Persistent job task that splits work into several chunks.
//              Handles restarts and works in parallel.
// Author:      Piotr Likus
// Modified by:
// Created:     01/06/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "sc/log.h"
#include "dtp/dnode_serializer.h"
#include "grd/JobWorkerTaskForSplitJoin.h"

using namespace dtp;

#define SPLITJOIN_DEBUG

// ----------------------------------------------------------------------------
// scMessagePackForSplitJoin
// ----------------------------------------------------------------------------
class scMessagePackForSplitJoin: public scMessagePack 
{
public:
  scMessagePackForSplitJoin(scJobWorkerTaskForSplitJoin *owner, scScheduler *scheduler): 
    scMessagePack() 
  {
    m_owner = owner;
    m_scheduler = scheduler;
  }

  virtual void beforeTaskDelete(const scTaskIntf *task, bool &handlerForDelete) {
    if (task == m_owner) {
      handlerForDelete = true;
    }  
  }
    
  virtual void handleAllReceived() {
    scString errCntTxt = toString(m_received.size());

#ifdef SPLITJOIN_DEBUG
    if (isResultOK())
      scLog::addDebug("scMessagePackForSplitJoin - all responses ("+errCntTxt+") received (OK)");
    else  
      scLog::addDebug("scMessagePackForSplitJoin - all responses ("+errCntTxt+") received (errors)");
#endif
      
    if (isResultOK()) {
      scDataNode fullResult;
      getFullResult(fullResult);   
      consumeResult(fullResult);
    } else {
      m_owner->handleMessagePackErrors(this); 
    }     
  }
  
  void consumeResult(const scDataNode &result)
  {
    scDataNode elementResult;
    std::map<unsigned int,int> resultMap;

    for(int i=0,epos=result.size(); i!=epos; i++)
    {
      result.getElement(i, elementResult);
      if (elementResult.hasChild("chunk_id"))
        resultMap.insert(std::make_pair<unsigned int,int>(elementResult.getUInt("chunk_id"), i));
    }
    
    
    std::map<unsigned int,int>::iterator p;

    for(p = resultMap.begin(); p != resultMap.end(); p++)
    {
#ifdef SPLITJOIN_DEBUG
      scLog::addInfo("Found result for chunk: "+toString(p->first));
#endif      
      result.getElement(p->second, elementResult);

#ifdef SPLITJOIN_DEBUG
      scLog::addDebug("result value: ["+elementResult.dump()+"]");             
#endif            
      m_owner->handleWorkerResult(p->first, elementResult);
    }//for p
    
    m_owner->handleMessagePackProcessed(); 
  } // function
private:
  scJobWorkerTaskForSplitJoin *m_owner;
  scScheduler *m_scheduler;
};


// ----------------------------------------------------------------------------
// scJobWorkerTaskForSplitJoin
// ----------------------------------------------------------------------------
scJobWorkerTaskForSplitJoin::scJobWorkerTaskForSplitJoin(scMessage *message): scJobWorkerTask(message) {
  m_chunkOffset = 0;
  m_nextChunkOffset = 0;
  m_restarted = false;
};

void scJobWorkerTaskForSplitJoin::intStartWork()
{
  scDataNode &params = getJobParams();
  m_workQueueName = params.getString("worker", "@worker");
  m_chunksPerCommit = params.getUInt("chunks_per_commit", 4);
  
  readConfig(params);

  scJobWorkerTask::intStartWork();
}

void scJobWorkerTaskForSplitJoin::readConfig(const scDataNode &jobParams) 
{
  scString configFileName = jobParams.getString("config_path", "");
  if (configFileName.length() > 0)
    readConfig(configFileName, m_config);
}

void scJobWorkerTaskForSplitJoin::readConfig(const scString &path, scDataNode &output)
{
  dnSerializer serializer;
  scString contents;
  serializer.setCommentsEnabled(true);
  readTextFileToString(path, contents);
  if (!serializer.convFromString(contents, output))
    throw scError(scString("Failed to read config file: ")+path);
} 

/// all the work is performed by worker nodes so here - just sleep forever
int scJobWorkerTaskForSplitJoin::runStep()
{
  if (!isSleeping())
    sleepFor(1000);
  return 0;
}

void scJobWorkerTaskForSplitJoin::handleMessagePackErrors(scMessagePack *pack)
{
  endWork(JEC_WU_OTHER_ERROR, "Worker error found");   
}

void scJobWorkerTaskForSplitJoin::handleMessagePackProcessed()
{
  setVar("chunk_offset", toString(m_nextChunkOffset));
  if (inTransaction())
    commitWork();
  else
    handleWorkUnitReady();  
}

void scJobWorkerTaskForSplitJoin::postNotifyCmd(const scString &notifyAddr, const scString &notifyCmd)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope()); 
  envelopeGuard->setReceiver(scMessageAddress(notifyAddr));

  scDataNode params;
  params.addChild("command", new scDataNode(notifyCmd));
    
  std::auto_ptr<scMessage> messageGuard(new scMessage());
  messageGuard->setCommand("core.run_cmd");
  messageGuard->setParams(params);
  envelopeGuard->setEvent(messageGuard.release());  
  postWithSync(envelopeGuard.release());  
}

void scJobWorkerTaskForSplitJoin::afterSyncAction(int action, const scDataNode &context)
{
  scJobWorkerTask::afterSyncAction(action, context);  
  if (action == jwtsoCommit) 
    performAfterCommit();
  else if (action == jwtsoLoadState)   
    performAfterLoadState();
}

void scJobWorkerTaskForSplitJoin::performAfterLoadState()
{
  scDataNode jobParams = getJobParams();

  scString defText("0");
  scString oneAsText("1");
  m_chunkOffset = stringToUInt(getVar("chunk_offset", defText));
  m_restarted = stringToUInt(getVar("process_restarted", defText)) > 0;
  setVar("process_restarted", oneAsText);

  handleStateLoaded();
  
  if (!m_restarted) {
    initProcessing();
  } else {
    loadWorkUnitState();
    resumeProcessing();
  }
  
  if (!isEndOfWork())
    postNextPack(false);
  else 
    endWork();    
}

void scJobWorkerTaskForSplitJoin::handleStateLoaded()
{ // empty here
}

void scJobWorkerTaskForSplitJoin::performAfterCommit()
{
  handleWorkUnitReady();
}

void scJobWorkerTaskForSplitJoin::handleWorkUnitReady()
{
  m_chunkOffset = m_nextChunkOffset;  

  if (isEndOfWorkUnit()) {
    processWorkUnit();
    saveWorkUnitState();    
//    setVar("step_no", toString(m_stepNo));
  }  
  
  setVar("chunk_offset", toString(m_chunkOffset));

  if (!isEndOfWork())
    postNextPack(true);
  else
    endWork();  
}

void scJobWorkerTaskForSplitJoin::postNextPack(bool sync)
{
//  scDataNode &params = getJobParams();
  uint postedCount;

  postMessagePack(postedCount);
  m_nextChunkOffset = m_chunkOffset + postedCount;

  // sync file requests
  if (sync)
    syncPoint();
}

scMessagePack *scJobWorkerTaskForSplitJoin::newMessagePack()
{
  return new scMessagePackForSplitJoin(this, getScheduler());
}

void scJobWorkerTaskForSplitJoin::postMessagePack(uint &postedCount)
{
  std::auto_ptr<scMessagePack> packGuard;  
  packGuard.reset(newMessagePack());    
  fillMessagePack(*packGuard);  
  packGuard.release()->post(getScheduler());  
}

// fill chunk params, configure in-memory state variables, executed on first run
void scJobWorkerTaskForSplitJoin::initProcessing()
{ // empty here
}

// resume processing: load chunk params & state from state vars
void scJobWorkerTaskForSplitJoin::resumeProcessing()
{
}

// set variables accordingly
void scJobWorkerTaskForSplitJoin::processWorkUnit()
{
  m_chunkOffset = 0;
}


