/////////////////////////////////////////////////////////////////////////////
// Name:        TaskImpl.cpp
// Project:     grdLib
// Purpose:     Task object - processing entity.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// perf
#include "perf/time_utils.h"
#include "perf\Log.h"

// grd
#include "grd/TaskImpl.h"
#include "grd/MessageConst.h"
#include "grd/details/SchedulerImpl.h"

using namespace perf;

// ----------------------------------------------------------------------------
// scTask
// ----------------------------------------------------------------------------

scTask::scTask() 
{
  m_stepTimeslice = SC_DEF_STEP_TIMESLICE;
  m_lastTimesliceStart = 0;
  m_priority = 0; // no time slicing 
  m_status = tsCreated;
  stopSleep();
}

scTask::~scTask()
{
}

scTaskStatus scTask::getStatus() const
{
  return m_status;
}
  
void scTask::setStatus(scTaskStatus value)
{
  scTaskStatus oldValue = getStatus();
  intSetStatus(value);
  if ((getStatus() == value) && (oldValue != value))
    statusChanged();
}

uint scTask::getPriority() const
{
  return m_priority;
}
  
void scTask::setPriority(uint value)
{
  m_priority = value;
}

void scTask::setStatusSilent(scTaskStatus value)
{
  intSetStatus(value);
}

void scTask::intSetStatus(scTaskStatus value)
{
  m_status = value;
}

void scTask::init()
{
  if (getStatus() == tsCreated)
  {
    intInit();
    setStatus(tsStarting);
  }  
}

void scTask::dispose()
{
  intDispose();
}

void scTask::intInit()
{ // do nothing here
}

void scTask::intDispose()
{ // do nothing here
}

void scTask::requestStop()
{
  switch (getStatus()) 
  {
    case tsCreated: case tsStarting: case tsRunning: case tsBusy:    
      setStatus(tsStopping);
      break;
    default:
      // do nothing
      break;
  }      
}

void scTask::statusChanged()
{
  intStatusChanged();  
  if (getStatus() == tsDestroying)
    dispose();
  if (getStatus() == tsStopped)
    closeTask();
}

void scTask::closeTask()
{
  setStatus(tsDestroying);
  m_scheduler->deleteTask(this);
}

void scTask::intStatusChanged()
{ // do nothing here
}

bool scTask::acceptsMessage(const scString &command, const scDataNode &params)
{
  return false;
}  

int scTask::handleMessage(scEnvelope &envelope, scResponse &response)
{
  return SC_MSG_STATUS_UNK_MSG;
}

//int scTask::handleMessage(scMessage *message, scResponse &response)
//{
//  return SC_MSG_STATUS_UNK_MSG;
//}

int scTask::handleResponse(scMessage *message, scResponse &response)
{
  return SC_MSG_STATUS_UNK_MSG;
}

scString scTask::getName() const
{
  return m_name;
}
  
void scTask::setName(const scString &value)
{
  m_name = value;
}   

struct scTaskStatusKeeper: public scTaskStatusKeeperBase {
  scTaskStatusKeeper(scTask *owner, scTaskStatus oldStatus, scTaskStatus newStatus): 
    scTaskStatusKeeperBase(owner), m_oldStatus(oldStatus), m_newStatus(newStatus) {
      setStatus(newStatus);
    }
  virtual ~scTaskStatusKeeper() { if (getStatus() == m_newStatus) setStatus(m_oldStatus); }
protected:
  scTaskStatus m_oldStatus;
  scTaskStatus m_newStatus;  
};

int scTask::run()
{
  int res = 0;
  scTaskStatus currStatus = getStatus();
  
  switch (currStatus) 
  {
    case tsStarting: {
      try {
        scTaskStatusKeeper statusKeeper(this, currStatus, tsBusy);
        res = runStarting();
      } catch(...) {
        requestStop();
        throw;
      }  
      break;
    }  
    case tsRunning: {
      try {
        scTaskStatusKeeper statusKeeper(this, currStatus, tsBusy);
        res = runByTimeslice();
      } catch(...) {
        requestStop();
        throw;
      }  
      break;
    }  
    case tsStopping: 
      try {
        res = runStopping();
      } catch(...) {
        Log::addError("Unknown error during task stop");
      }  
      break;
    default:
      //do nothing
      res = 0;
      break;        
  }    
  return res;
}

int scTask::runByTimeslice()
{
  int res = 0;
  int stepRes;
  if (m_priority > 0) startTimeslice();
  do {
    stepRes = intRun();
    res += stepRes;
  } while ((stepRes > 0) && (m_priority > 0) && !isEndOfTimeslice());
  return res;
}

bool scTask::needsRun()
{
  scTaskStatus currStatus = getStatus();
  if (currStatus == tsRunning)
    return !isSleeping();
  else if (currStatus != tsBusy)
    return true;
  else    
    return false;
}

bool scTask::isDaemon()
{
  return true;
}

int scTask::intRun()
{
  return 0;
}  

int scTask::runStarting()
{
  setStatus(tsRunning);
  return 0;
}  
  
int scTask::runStopping()
{
  setStatus(tsStopped);
  return 0;
}

void scTask::setScheduler(scSchedulerIntf *scheduler)
{
  m_scheduler = scheduler;
}

scSchedulerIntf *scTask::getScheduler() const
{
  return m_scheduler;
}

scMessageAddress scTask::getOwnAddress(const scString &protocol)
{
  scMessageAddress res;
  
  if (m_scheduler != SC_NULL)
  {
    res = m_scheduler->getOwnAddress(protocol); 
    if (res.getFormat() != scMessageAddress::AdrFmtDefault)
      throw scError("Default address fmt required!");       
  }

  if (!getName().length())
    throw scError("Task name required");       
  
  res.setTask(getName());
  return res;
}

void scTask::sleepFor(cpu_ticks period_ms)
{
  m_sleepLength = period_ms;
  m_sleepStart = cpu_time_ms();
}

bool scTask::isSleeping()
{
  bool res;
  unsigned long currTime = cpu_time_ms();
  if (
       (m_sleepLength > 0) 
        &&       
       (
         (m_sleepStart + m_sleepLength > currTime)
         ||
         (currTime < m_sleepStart)
       )  
      )
    res = true;
  else
    res = false;  
  return res;
}

void scTask::stopSleep()
{ 
  m_sleepLength = 0;
}

void scTask::startTimeslice()
{
  m_lastTimesliceStart = cpu_time_ms();
}

bool scTask::isEndOfTimeslice()
{
  bool res;
  if (m_stepTimeslice > 0) {
    // priority 1 = 500ms, 5 = 100ms, 10 = 50, 20 = 25, 100 = 5
    cpu_ticks dynamicTimeslice = (m_stepTimeslice * 10 / m_priority); 
    if (dynamicTimeslice <= 0)
      dynamicTimeslice = 1;
    res = is_cpu_time_elapsed_ms(m_lastTimesliceStart, dynamicTimeslice);
  } else {
    res = false;   
  }
  return res;
}

int scTask::getNextRequestId()
{
  return dynamic_cast<scScheduler *>(getScheduler())->getNextRequestId();
}

