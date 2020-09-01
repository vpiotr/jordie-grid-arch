/////////////////////////////////////////////////////////////////////////////
// Name:        CompactServer.h
// Project:     grdLib
// Purpose:     Core class for grid-enabled application.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDCOMPACTSERVER_H__
#define _GRDCOMPACTSERVER_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file CompactServer.h
///
/// grdLib service runner class.
/// Can be used to start & handle grdLib services.
/// Can be embedded in user-defined application.
/// Override protected methods to define additional modules / gates.
///
/// Default server parts:
/// - scheduler (processing time distribution, message passing)
/// - gates:
///   - inproc
///   - Boost message queue
///   - ZeroMQ (tcp, pgm, ipc)
/// - modules:
///   - Core
///   - Simple Queue (squeue)
///   - listener (for handling messages by non-module classes)
///   - Watchdog
/// - other:
///   - local registry
///   - node factory, builds nodes with modules:
///     - Core
///     - Simple queue
///     - listener


// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "sc/events/Events.h"

#include "grd/core.h"
#include "grd/CommandParser.h"
#include "grd/ZeroMQGates.h"
#include "grd/LocalNodeRegistry.h"
#include "grd/NodeFactory.h"
#include "grd/Scheduler.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
typedef std::auto_ptr<zmContextBase> zmContextBaseGuard;

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class grdCompactNodeFactory;

class grdCompactServer {
public:
  // construction
  grdCompactServer();
  virtual ~grdCompactServer();
  virtual void init();
  // execution
  virtual void parseCommandLine(int argc, char* argv[]);
  virtual void parseCommandLine(const scDataNode &args);
  virtual void parseCommandList(const scString &line);
  virtual void run();
  virtual void runYieldBusy();
  virtual void runYieldWait();
  virtual void runStep();
  bool needsRun();
  virtual void requestStop();
  virtual void waitForStop();
  void addInterfaceToObserved(const scString &intfName);
  // properties
  scSchedulerIntf *getScheduler();
  void setStopOnIdle(bool value);
  virtual void setCommandNotifier(scNotifier *notifier);
  bool isBusy();
protected:
  virtual void initGates();
  virtual void initModules();
  virtual void initCoreModule();
  virtual void initLocalRegistry();
  virtual void initSimleQueueModule();
  virtual void initListenerModule();
  virtual void initWatchdogModule();
  virtual void initScheduler();
  virtual void initZeroMQ();
  bool runSchedulers();
  int getRunningSchedulerCount();
  uint getUserTaskCount();
  virtual grdCompactNodeFactory *createNodeFactory();
  virtual void initNodeFactory();
  virtual void stepPerformed();
  virtual void performYieldOutIdle(uint64 timeMs = 0);
  virtual void performYieldOutBusy();
  void checkYieldInterval();
protected:
  bool m_stopOnIdle;
  scCommandParser *m_commandParser;
  std::auto_ptr<scScheduler> m_scheduler;
  scLocalNodeRegistry m_localRegistry;
  scModuleMap m_handlers;
  zmContextBaseGuard m_zmContext;
  std::auto_ptr<grdCompactNodeFactory> m_nodeFactory;
  cpu_ticks m_lastYield;  
  cpu_ticks m_lastYieldOut;  
};

class grdCompactNodeFactory: public scNodeFactory
{
public:
  grdCompactNodeFactory();
  virtual void setCommandNotifier(scNotifier *notifier);
protected:
  virtual scSchedulerIntf *intCreateNode(const scString &a_className, const scString &nodeName);
  virtual scString getClassName() const;
  virtual void initModules(scSchedulerIntf *scheduler);
protected:
  scModuleColn m_modules;
  scNotifier *m_commandNotifier;
};

class grdYieldSignalForCompactSrv: public scYieldSignal {
public:
  grdYieldSignalForCompactSrv(grdCompactServer *server, uint interval = 0);
  virtual ~grdYieldSignalForCompactSrv();
protected:
  virtual void invokeYield();
protected:
  grdCompactServer *m_server;
};

#endif // _GRDCOMPACTSERVER_H__