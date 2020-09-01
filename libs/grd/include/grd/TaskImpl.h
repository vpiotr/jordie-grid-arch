/////////////////////////////////////////////////////////////////////////////
// Name:        TaskImpl.h
// Project:     grdLib
// Purpose:     Task interface implemention.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDTASKIMPL_H__
#define _GRDTASKIMPL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file TaskImpl.h
\brief Short file description

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/Task.h"
#include "grd/Scheduler.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const unsigned long SC_DEF_STEP_TIMESLICE = 20; // ms

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scTask
// ----------------------------------------------------------------------------
/// A process that is performed
class scTask: public scTaskIntf
{
public:
    scTask();
    virtual ~scTask();
    // --- interface
    virtual int handleMessage(scEnvelope &envelope, scResponse &response);    
    //virtual int handleMessage(scMessage *message, scResponse &response);    
    virtual int handleResponse(scMessage *message, scResponse &response);
    virtual bool acceptsMessage(const scString &command, const scDataNode &params);
    virtual scString getName() const;
    virtual void setName(const scString &value);
    /// returns >1 if there is a need for more iterations
    virtual int run(); 
    virtual bool needsRun();
    virtual void requestStop();
    virtual bool isDaemon();    
    // --- other
    /// performed for task dynamic initialization
    void init(); 
    /// performed for task cleaning / before destroying
    void dispose();  
    /// returns <true> if task runs indefinitely in background (as a service)
    void setScheduler(scSchedulerIntf *scheduler);
    scSchedulerIntf *getScheduler() const;
    virtual scMessageAddress getOwnAddress(const scString &protocol);
    virtual scTaskStatus getStatus() const;
    void setStatus(scTaskStatus value);
    uint getPriority() const;
    void setPriority(uint value);
protected:    
    virtual void intInit(); 
    virtual void intDispose(); 
    /// returns >1 if there is a need for more iterations
    virtual int intRun(); 
    int runByTimeslice();
    virtual int runStarting(); 
    virtual int runStopping(); 
    virtual void intSetStatus(scTaskStatus value);
    virtual void closeTask();
    void statusChanged();
    virtual void intStatusChanged();
    void sleepFor(cpu_ticks period_ms);
    bool isSleeping();
    void stopSleep();
    void startTimeslice();
    bool isEndOfTimeslice();
    void setStatusSilent(scTaskStatus value);
    int getNextRequestId(); 
private:
    scString m_name;    
    scSchedulerIntf *m_scheduler;
    scTaskStatus m_status;
    unsigned long m_sleepStart;
    unsigned long m_sleepLength;
    uint m_priority; 
    cpu_ticks m_stepTimeslice;
    cpu_ticks m_lastTimesliceStart;
    friend struct scTaskStatusKeeperBase;
};

struct scTaskStatusKeeperBase {
  scTaskStatusKeeperBase(scTask *owner): m_owner(owner) {} 
  virtual ~scTaskStatusKeeperBase() {}
protected:
  virtual scTaskStatus getStatus() { return m_owner->getStatus(); }
  virtual void setStatus(scTaskStatus value) { m_owner->setStatusSilent(value); }
private:
  scTask *m_owner;  
};


#endif // _GRDTASKIMPL_H__