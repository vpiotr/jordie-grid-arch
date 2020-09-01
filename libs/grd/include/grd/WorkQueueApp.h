/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueApp.h
// Project:     grdLib
// Purpose:     Universal work queue application object
//              Ready to be used as container for worker & client.
// Author:      Piotr Likus
// Modified by:
// Created:     26/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _WORKQUEUEAPP_H__
#define _WORKQUEUEAPP_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
/// App usage scenario:
///
/// ... init clientTask
/// app.setCliArgs(args)
/// app.addTask(clientTask)
/// [app.setModuleFactory(workModuleFactory)]
/// app.run();
/// 
/// ... clientTask.yield() -> app.yield() -> run workers|sleep k msec

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/events/Events.h"
#include "grd/core.h"
#include "grd/TaskImpl.h"
#include "grd/WorkQueueClient.h"
#include "grd/WorkQueueWorker.h"

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
class scWqAppBody {
public:
  scWqAppBody() {}
  virtual ~scWqAppBody() {}
};

class scWqApp {
public:
// construct
  scWqApp();  
  virtual ~scWqApp();
// properties
  void setCliArgs(const scDataNode &args);
  void setStopOnIdle(bool value);  
  void addTask(scTask *task);
  void addWorker(scWqWorker *worker);
  virtual scWqServerProxy *prepareServerProxy();
  virtual scSignal *prepareYieldSignal(bool withDelay);
  //void addWorkHandler(scWqWorkHandler *handler);
  //void addWorkModule(scWqWorkModule *handler);
// run
  void init(); 
  void run();
protected:
  virtual scWqAppBody *newBody();
  scWqAppBody *prepareBody();
protected:
  std::auto_ptr<scWqAppBody> m_body;
};

/*
class scWqWorkHandler {
public:
  void getHandledRequests(scStringList &output);
};

typedef boost::ptr_map<scString, scWqWorkHandler> scWqWorkHandlerMap;

class scWqWorkModule: public scModule {
public:
  void addHandler(scStringList &commandList, scWqWorkHandler *handler);
  virtual int handleMessage(scMessage *message, scResponse &response);
  virtual scStringList supportedInterfaces() const;
protected:
  scWqWorkHandlerMap m_handlers;  
  scStringList m_supportedIntefaces;
};
*/

class scWqYieldSignal: public scSignal {
public:
  scWqYieldSignal(scNotifier *notifier, uint interval = 0);
  virtual ~scWqYieldSignal();
  void setInterval(uint value);
  virtual void execute();
protected:  
  virtual void invokeYield();
protected:
  uint m_interval;  
  cpu_ticks m_lastYieldEnd;
};

//----------------------------------------------------------------
// Client part - for modal tasks
//----------------------------------------------------------------

class scWqModalTaskIntf {
public:
  scWqModalTaskIntf() {}
  virtual ~scWqModalTaskIntf() {}
  
  virtual void run() = 0;
};

class scWqTaskContainerForModalTask: public scTask {
public:
  scWqTaskContainerForModalTask(scWqModalTaskIntf *ownedTask): scTask() { m_ownedTask.reset(ownedTask); }    
  virtual ~scWqTaskContainerForModalTask() {}
protected:
  virtual int intRun();
  virtual bool isDaemon() { return false; }
protected:
  std::auto_ptr<scWqModalTaskIntf> m_ownedTask; 
};

#endif // _WORKQUEUEAPP_H__