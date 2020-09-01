/////////////////////////////////////////////////////////////////////////////
// Name:        CompactServer.cpp
// Project:     grdLib
// Purpose:     Core class for grid-enabled application.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// --- use to perform sleep during busy moments
//#define USE_YIELD_OUT_ON_BUSY

// boost
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include <boost/thread.hpp>

// sc
#include "sc/proc/process.h"

#include "perf/Log.h"
#include "perf/Timer.h"
#include "perf/Counter.h"

#include "grd/core.h"
#include "grd/CommandParser.h"
#include "grd/CoreModule.h"
#include "grd/SmplQueue.h"
#include "grd/details/SchedulerImpl.h"
#include "grd/Scheduler.h"
#include "grd/MessageConst.h"
#include "grd/EventTrace.h"

#ifdef GRD_USE_ZEROMQ
#include "grd/ZeroMQGates.h"
#endif

#ifdef GRD_USE_BOOST_MSG_QUEUE
#include "grd/BoostMsgQueueGate.h"
#endif

#ifdef GRD_USE_NAMED_PIPES_QUEUE
#include "grd/W32NamedPipesGate.h"
#endif

#include "grd/W32Watchdog.h"

#include "grd/CompactServer.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace perf;

const uint YIELD_WARNING_INTERVAL_MS = 30*1000;
const uint YIELD_OUT_IDLE_DELAY_MS = 1;
const uint YIELD_OUT_BUSY_DELAY_MS = 150;

//const uint YIELD_WAIT_MS = 1;
const uint YIELD_WAIT_CLI_DELAY_MS = 1;
const uint YIELD_BUSY_DELAY_MS = 150;

// minimum time for sleep on yield during wait
const cpu_ticks YIELD_WAIT_MIN_SLEEP_TIME_MS = 1;
// maximum time for sleep on yield during wait
const cpu_ticks YIELD_WAIT_MAX_SLEEP_TIME_MS = 100;

// default sleep time
const cpu_ticks YIELD_DEF_SLEEP_TIME_MS = 1;

// sleep time / scheduler time ratio
const double YIELD_WAIT_SLEEP_SCHEDULER_RATIO = 9;

const cpu_ticks GRD_WAIT_WORKER_SLEEP_TIME = 10;
const cpu_ticks GRD_WAIT_CLI_SLEEP_TIME = 1;
const uint GRD_WAIT_WORKER_CNT_FOR_SLEEP_START = 30;

//-------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------
bool checkDelayedProcessStart(cpu_ticks &lastRunDt, cpu_ticks delayMSecs)
{
  return ((lastRunDt == 0) || is_cpu_time_elapsed_ms(lastRunDt, delayMSecs));
}

void signalDelayedProcessEnd(cpu_ticks &lastRunDt)
{
  lastRunDt = cpu_time_ms();
}

double calcAvgTime(const char *metricName)
{
  cpu_ticks time = Timer::getTotal(metricName);
  uint64 cnt = Counter::getTotal(metricName);
  if (cnt == 0)
    cnt = 1;
  return static_cast<double>(time) / static_cast<double>(cnt);
}

cpu_ticks calcSleepTimeForWait(const char *schedulerMetricName, cpu_ticks lastSchedTime = 0)
{
  const double AVG_FACTOR = 0.1;
  double avgSchedulerTime = calcAvgTime(schedulerMetricName);
  double lastSchedulerTimeDbl; 
  cpu_ticks sleepTime;

  if (lastSchedTime > 0)
    lastSchedulerTimeDbl = static_cast<double>(lastSchedTime);
  else 
    lastSchedulerTimeDbl = avgSchedulerTime;

  sleepTime = round<cpu_ticks>(((avgSchedulerTime * AVG_FACTOR) + (lastSchedulerTimeDbl * (1.0 - AVG_FACTOR)) * YIELD_WAIT_SLEEP_SCHEDULER_RATIO));

  if (sleepTime < YIELD_WAIT_MIN_SLEEP_TIME_MS)
    sleepTime = YIELD_WAIT_MIN_SLEEP_TIME_MS;
  if (sleepTime > YIELD_WAIT_MAX_SLEEP_TIME_MS)
    sleepTime = YIELD_WAIT_MAX_SLEEP_TIME_MS;

  return sleepTime;
}

//-------------------------------------------------------------------------------
// Private classes
//-------------------------------------------------------------------------------
/// Module used for handling messages without modules.
///
/// Example usage: (abstract worker class for handling messages)
/// - set server's command notifier
///   - compactServer->setCommandNotifier
/// - define worker class
/// - define worker proxy class for listening to messages (scListener)
/// - add worker proxy object as listener to notifier
///   - notifier->addListener
/// - add worker interface to list of interfaces that server listens to
///   - compactServer->addInterfaceToObserved
///
class grdListenerModule: public scModule {
public:
  grdListenerModule();
  void setNotifier(scNotifier *notifier);
  virtual int handleMessage(scMessage *message, scResponse &response);
  void registerInterface(const scString &name);
  virtual scStringList supportedInterfaces() const;
protected:
  int invokeNotifier(scMessage *message, scResponse &response);
protected:
  scNotifier *m_notifier;
  scStringList m_interfaces;
};

//-------------------------------------------------------------------------------
// grdListenerModule
//-------------------------------------------------------------------------------
grdListenerModule::grdListenerModule(): scModule(), m_notifier(SC_NULL)
{
}

void grdListenerModule::setNotifier(scNotifier *notifier)
{
  m_notifier = notifier;
}

int grdListenerModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = 0;

  try {
    res = invokeNotifier(message, response);
  }
  catch(scError &excp) {
    scString msg = scString("GLM01: Exception (scError): ") + excp.what()+", code: "+toString(excp.getErrorCode())+", details: "+excp.getDetails();
    Log::addError(msg);
    res = SC_MSG_STATUS_EXCEPTION;
    response.setError(scDataNode(msg));
  }
  catch(const std::exception& e) {
    scString msg = scString("GLM02: Exception (std): ") + e.what();
    Log::addError(msg);
    res = SC_MSG_STATUS_EXCEPTION;
    response.setError(scDataNode(msg));
  }
  catch(...) {
    scString msg = "GLM03: Warning: exception (unknown type)";
    Log::addError(msg);
    res = SC_MSG_STATUS_EXCEPTION;
    response.setError(scDataNode(msg));
  }
  response.setStatus(res);
  return res;
}

int grdListenerModule::invokeNotifier(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_CFG;
  if (m_notifier != SC_NULL)
  {
    scDataNode output;

    scDataNode params = message->getParams();
    scString command = message->getCommand();

    if (m_notifier->invoke(command, &params, &output))
    {
      response.setResult(base::move(output));
      res = SC_MSG_STATUS_OK;
    } else {
      res = SC_MSG_STATUS_UNK_MSG;
    }
  }
  return res;
}


void grdListenerModule::registerInterface(const scString &name)
{
  if (find(m_interfaces.begin(), m_interfaces.end(), name) == m_interfaces.end())
    m_interfaces.push_back(name);
}

scStringList grdListenerModule::supportedInterfaces() const
{
  return m_interfaces;
}

//-------------------------------------------------------------------------------
// grdCompactServer
//-------------------------------------------------------------------------------
grdCompactServer::grdCompactServer()
{
  m_stopOnIdle = false;
  m_scheduler.reset(scScheduler::newScheduler());
  m_scheduler->setName("main");
  m_commandParser = new scCommandParser();
  m_commandParser->setScheduler(m_scheduler.get());
  m_lastYield = m_lastYieldOut = 0;
}

grdCompactServer::~grdCompactServer()
{
  m_commandParser->setScheduler(SC_NULL);
  //delete m_scheduler;
  delete m_commandParser;
  //DEBUG:+
  m_localRegistry.clear();
  m_zmContext.reset();
  m_handlers.clear();
  m_nodeFactory.reset();
  //DEBUG:-
}

void grdCompactServer::setStopOnIdle(bool value)
{
  m_stopOnIdle = value;
}

void grdCompactServer::init()
{
  initNodeFactory();
  initModules();
  initGates();
  initScheduler();
  initLocalRegistry();
}

void grdCompactServer::run()
{
  bool schedulersBusy;
  cpu_ticks sleepTime;
  uint waitCnt = 0;
  do {
    cpu_ticks startTime = Timer::getTotal("loc-sched-main");
    Timer::start("loc-sched-main");

    //schedulersBusy = 
    runSchedulers();

    Timer::stop("loc-sched-main");
    cpu_ticks schedTime = Timer::getTotal("loc-sched-main") - startTime;

    schedulersBusy = isBusy();

    if (schedulersBusy) {
      Timer::inc("loc-sched-main-busy", schedTime);
      Counter::inc("loc-sched-main-busy");
      waitCnt = 0;
    } else {
      Timer::inc("loc-sched-main-wait", schedTime);
      Counter::inc("loc-sched-main-wait");
      waitCnt++;
    }

    if ((!schedulersBusy) && (waitCnt > GRD_WAIT_WORKER_CNT_FOR_SLEEP_START)) {
      //sleepTime = calcSleepTimeForWait("loc-sched-main-wait", schedTime);
      sleepTime = GRD_WAIT_WORKER_SLEEP_TIME;
      performYieldOutIdle(sleepTime);
    }
    stepPerformed();
  } while(needsRun());
}

bool grdCompactServer::isBusy()
{
  return (getUserTaskCount() > 0);
}

void grdCompactServer::runStep()
{
  grdEventTrace::addStep("before-run-step", "compact-server", 1);
  bool performYield = !runSchedulers();
  if (performYield)
    performYieldOutIdle();
  stepPerformed();
  grdEventTrace::addStep("after-run-step", "compact-server", 1);
}

// accept CPU time from external processes (during processing)
void grdCompactServer::runYieldBusy()
{
  if (!checkDelayedProcessStart(m_lastYield, YIELD_BUSY_DELAY_MS))
    return;

  grdEventTrace::addStep("before-yield-busy", "compact-server", 1);
  checkYieldInterval();
  Timer::start("grd-yield-busy");
  Timer::start("grd-yield-sched");
  Timer::start("grd-yield-sched-busy");
  runSchedulers();
  Timer::stop("grd-yield-sched-busy");
  Timer::stop("grd-yield-sched");
  Timer::start("grd-yield-notify");
  stepPerformed();
  Timer::stop("grd-yield-notify");
#ifdef USE_YIELD_OUT_ON_BUSY
  performYieldOutBusy();
#endif
  Timer::stop("grd-yield-busy");
  Counter::inc("grd-yield-busy");
  grdEventTrace::addStep("after-yield-busy", "compact-server", 2);
  //m_lastYield = cpu_time_ms();
  signalDelayedProcessEnd(m_lastYield);
}

void grdCompactServer::checkYieldInterval()
{
  if (m_lastYield != 0) {
    if (is_cpu_time_elapsed_ms(m_lastYield, YIELD_WARNING_INTERVAL_MS)) {
      Log::addWarning("Yield interval too large - add yield to processing steps");
    }
  }
}

// accept CPU time from external processes (during waiting)
void grdCompactServer::runYieldWait()
{
  if (!checkDelayedProcessStart(m_lastYield, YIELD_WAIT_CLI_DELAY_MS))
    return;

  cpu_ticks sleepTime;
  
  //sleepTime = calcSleepTimeForWait("grd-yield-sched-wait");
  sleepTime = GRD_WAIT_CLI_SLEEP_TIME;

  grdEventTrace::addStep("before-yield-wait", "compact-server", 1);
  Timer::start("grd-yield-wait");
  checkYieldInterval();
  Timer::start("grd-yield-sched");
  Timer::start("grd-yield-sched-wait");
  runSchedulers();
  Timer::stop("grd-yield-sched-wait");
  Counter::inc("grd-yield-sched-wait");
  Timer::stop("grd-yield-sched");
  stepPerformed();
  if (!isBusy())
    performYieldOutIdle(sleepTime);
  Timer::stop("grd-yield-wait");
  Counter::inc("grd-yield-wait");

  grdEventTrace::addStep("after-yield-wait", "compact-server", 2);
  //m_lastYield = cpu_time_ms();
  signalDelayedProcessEnd(m_lastYield);
}

// give time for other processes & tasks
void grdCompactServer::performYieldOutIdle(uint64 timeMs)
{
  if (timeMs == 0)
    timeMs = YIELD_DEF_SLEEP_TIME_MS;

  grdEventTrace::addStep("before-yield-out-idle", "compact-server", 1);
  //if ((m_lastYieldOut == 0) || is_cpu_time_elapsed_ms(m_lastYieldOut, YIELD_OUT_IDLE_DELAY_MS)) 
  if (checkDelayedProcessStart(m_lastYieldOut, YIELD_OUT_IDLE_DELAY_MS))
  {
    Timer::start("grd-yield-thread-idle");
    boost::this_thread::yield();
    Timer::stop("grd-yield-thread-idle");
    Timer::start("grd-yield-sleep-idle");
    //proc::sleepProcess(1);
    proc::sleepThisThreadMs(timeMs);
    Timer::stop("grd-yield-sleep-idle");
    //m_lastYieldOut = cpu_time_ms();
    signalDelayedProcessEnd(m_lastYieldOut);
  }
  grdEventTrace::addStep("after-yield-out-idle", "compact-server", 2);
}

void grdCompactServer::performYieldOutBusy()
{
  grdEventTrace::addStep("before-yield-out-busy", "compact-server", 1);

  //if ((m_lastYieldOut == 0) || is_cpu_time_elapsed_ms(m_lastYieldOut, YIELD_OUT_BUSY_DELAY_MS)) 
  if (checkDelayedProcessStart(m_lastYieldOut, YIELD_OUT_BUSY_DELAY_MS))
  {
    Timer::start("grd-yield-thread-busy");
    boost::this_thread::yield();
    Timer::stop("grd-yield-thread-busy");
    Timer::start("grd-yield-sleep-busy");
    proc::sleepProcess(1);
    Timer::stop("grd-yield-sleep-busy");
    //m_lastYieldOut = cpu_time_ms();
    signalDelayedProcessEnd(m_lastYieldOut);
  }

  grdEventTrace::addStep("after-yield-out-busy", "compact-server", 2);
}

void grdCompactServer::stepPerformed()
{ //empty
}

bool grdCompactServer::needsRun()
{
  bool res;
  if (m_stopOnIdle && (getUserTaskCount() == 0))
    res = false;
  else
    res = (getRunningSchedulerCount() > 0);
  return res;
}

void grdCompactServer::requestStop()
{
  for( scLocalNodeRegistryIterator i = m_localRegistry.begin();
       i != m_localRegistry.end(); ++i )
  {
    (*i->second).requestStop();
  }
}

void grdCompactServer::waitForStop()
{
    while(needsRun()) {
        runSchedulers();
    }
}


bool grdCompactServer::runSchedulers()
{
  bool res = false;

  for( scLocalNodeRegistryIterator i = m_localRegistry.begin();
       i != m_localRegistry.end(); ++i )
  {
    try {
      (*i->second).run();
      res = res || (*i->second).needsRun();
    }
    catch (scError &e) {
      scString msg = e.getDetails();
      if (msg.empty())
        msg = e.what();
      else
        msg = scString(e.what())+scString(", details: [")+msg+scString("]");
      Log::addError(msg);
    }
  }
  return res;
}

int grdCompactServer::getRunningSchedulerCount()
{
  int res = 0;

  for( scLocalNodeRegistryIterator it = m_localRegistry.begin(), epos = m_localRegistry.end();
       it != epos; ++it )
  {
    if ((*it->second).getStatus() != tsStopped)
    {
        ++res;
    }
  }

  return res;
}

/// returns number of tasks that are not deamons (permament) for all schedulers
uint grdCompactServer::getUserTaskCount()
{
  uint res = 0;

  for( scLocalNodeRegistryIterator it = m_localRegistry.begin(), epos = m_localRegistry.end();
       it != epos; ++it )
  {
    if ((*it->second).getStatus() != tsStopped)
    {
      res += (*it->second).getNonDeamonTaskCount();
    }
  }
  return res;
}


scSchedulerIntf *grdCompactServer::getScheduler()
{
  scSchedulerIntf *res = m_scheduler.get();
  if (res == SC_NULL)
  {
    if (!m_localRegistry.empty())
      res = m_localRegistry.begin()->second;
  }
  return res;
}

void grdCompactServer::initGates()
{
  scCoreModule *coreModule = dynamic_cast<scCoreModule *>(m_handlers.find("core")->second);
#ifdef GRD_USE_ZEROMQ
  initZeroMQ();

  coreModule->registerGateFactory("tcp", new zmGateFactoryForTcp(m_zmContext.get()));
  coreModule->registerGateFactory("pgm", new zmGateFactoryForPgm(m_zmContext.get()));
  coreModule->registerGateFactory("ipc", new zmGateFactoryForIpc(m_zmContext.get()));
#endif

#ifdef GRD_USE_BOOST_MSG_QUEUE
  coreModule->registerGateFactory("bmq", new grdBmqGateFactory());
#endif

#ifdef GRD_USE_NAMED_PIPES_QUEUE
  coreModule->registerGateFactory("npq", new grdW32NamedPipesGateFactory());
#endif
}

void grdCompactServer::setCommandNotifier(scNotifier *notifier)
{
  grdListenerModule *listenerModule = dynamic_cast<grdListenerModule *>(m_handlers.find("listen")->second);
  listenerModule->setNotifier(notifier);
  if (m_nodeFactory.get() != SC_NULL)
    m_nodeFactory->setCommandNotifier(notifier);
}

void grdCompactServer::addInterfaceToObserved(const scString &intfName)
{
  grdListenerModule *listenerModule = dynamic_cast<grdListenerModule *>(m_handlers.find("listen")->second);
  listenerModule->registerInterface(intfName);
}

void grdCompactServer::initZeroMQ()
{
#ifdef GRD_USE_ZEROMQ
  m_zmContext.reset(zmContextBase::newContext());
#endif
}

void grdCompactServer::initScheduler()
{
  m_scheduler->init();
}

void grdCompactServer::initModules()
{
  initCoreModule();
  initSimleQueueModule();
  initListenerModule();
  initWatchdogModule();
}

void grdCompactServer::initCoreModule()
{
  std::auto_ptr<scCoreModule> coreModuleGuard(new scCoreModule());
  coreModuleGuard->setScheduler(dynamic_cast<scScheduler *>(m_scheduler.get()));
  coreModuleGuard->setCommandParser(m_commandParser);
  m_scheduler->addModule(coreModuleGuard.get());
  scString mname("core");
  m_handlers.insert(mname, coreModuleGuard.release());
}

void grdCompactServer::initSimleQueueModule()
{
  std::auto_ptr<scModule> guardModule(new scSmplQueueModule());
  m_scheduler->addModule(guardModule.get());
  scString mname("squeue");
  m_handlers.insert(mname, guardModule.release());
}

void grdCompactServer::initListenerModule()
{
  std::auto_ptr<scModule> guardModule(new grdListenerModule());
  m_scheduler->addModule(guardModule.get());
  scString mname("listen");
  m_handlers.insert(mname, guardModule.release());
}

void grdCompactServer::initWatchdogModule()
{
  std::auto_ptr<scModule> guardWatchdog(new scW32WatchdogModule());
  m_scheduler->addModule(guardWatchdog.get());
  scString mname("watchdog");
  m_handlers.insert(mname, guardWatchdog.release());
}

void grdCompactServer::initLocalRegistry()
{
  m_scheduler->setLocalRegistry(&m_localRegistry);
  m_localRegistry.add(m_scheduler.release());
}

void grdCompactServer::parseCommandLine(int argc, char* argv[])
{
  scDataNode args;
  scString arg, value, name;
  size_t kpos;

  args.setAsList();

  for(int i=0; i<argc; ++i)
  {
    arg = argv[i];
    kpos = arg.find(":");

    if (kpos != scString::npos)
    {
      if ((arg.find('-') == 0) || (arg.find('/') == 0))
        name = arg.substr(1, kpos - 1);
      else
        name = arg.substr(0, kpos);
      value = arg.substr(kpos + 1);
    } else {
      if ((arg.find('-') == 0) || (arg.find('/') == 0))
        name = arg.substr(1);
      else
        name = arg;
      value = "";
    }
    args.addElement(name, scDataNode(value));
  }

  {
      namespace fs = boost::filesystem;

      if (argc > 0) {
        fs::path full_path( fs::initial_path<fs::path>() );
        full_path = fs::system_complete( fs::path( argv[0] ) );
        full_path.make_preferred();
        scString stds(full_path.string());

        //args.addElement(scDataNode("exec_path", stdStringToString(full_path.string())));
        //args.addElement(scDataNode("exec_path", stdStringToString(full_path.native_file_string())));        
        args.addElement("exec_path", scDataNode(stdStringToString(stds)));        
      }
  }

  parseCommandLine(args);
}

void grdCompactServer::parseCommandLine(const scDataNode &args)
{
  const scString runParamToken("ri");
  const scString runScriptParam("rs");
  scString arg, argValue;

  if (args.size() > 1)
  {
    //Log::addDebug(scString("Exec path: ")+args.getString("exec_path"));
    m_commandParser->setExecPath(args.getString("exec_path"));
    for(int i=1, epos = args.size(); i < epos; ++i)
    {
      arg = args.getElementName(i);
      if (arg == runParamToken)
      {
        argValue = args.getString(i);
#ifdef SC_LOG_ENABLED
        Log::addText("Command line: "+argValue);
#endif
        parseCommandList(argValue);
      } else if (arg == runScriptParam) {
        argValue = args.getString(i);
        argValue = scString("run '") + argValue + "'";
#ifdef SC_LOG_ENABLED
        Log::addText("Command line: "+argValue);
#endif
        parseCommandList(argValue);
      }
    }
  }
}

void grdCompactServer::parseCommandList(const scString &line)
{
  try {
    m_commandParser->parseCommand(line);
  }
  catch (scError &e) {
    Log::addError(e.what());
  }
}

void grdCompactServer::initNodeFactory()
{
  if (!scNodeFactory::factoryExists())
    m_nodeFactory.reset(createNodeFactory());
}

grdCompactNodeFactory *grdCompactServer::createNodeFactory()
{
  return new grdCompactNodeFactory;
}

//-------------------------------------------------------------------------------
// grdCompactNodeFactory
//-------------------------------------------------------------------------------
grdCompactNodeFactory::grdCompactNodeFactory(): scNodeFactory(), m_commandNotifier(SC_NULL)
{
}

void grdCompactNodeFactory::setCommandNotifier(scNotifier *notifier)
{
  m_commandNotifier = notifier;
}

scSchedulerIntf *grdCompactNodeFactory::intCreateNode(const scString &a_className, const scString &nodeName)
{
  std::auto_ptr<scSchedulerIntf> guard;
  scSchedulerIntf *res = SC_NULL;

  if (a_className == getClassName())
  {
    guard.reset(scScheduler::newScheduler());
    res = guard.get();

    res->setName(nodeName);

    initModules(res);
  } else {
    throw scError("Unknown node class name: ["+a_className+"]");
  }

#ifdef SC_LOG_ENABLED
  Log::addText("Node created for ["+a_className+", "+nodeName+"]");
#endif
  return guard.release();
}

scString grdCompactNodeFactory::getClassName() const
{
  return scString("base");
}

void grdCompactNodeFactory::initModules(scSchedulerIntf *scheduler)
{
  std::auto_ptr<scModule> guardCoreModule(new scCoreModule());
  scheduler->addModule(guardCoreModule.get());
  m_modules.push_back(guardCoreModule.release());

  std::auto_ptr<scModule> guardSmplQueueModule(new scSmplQueueModule());
  scheduler->addModule(guardSmplQueueModule.get());
  m_modules.push_back(guardSmplQueueModule.release());

  if (m_commandNotifier != SC_NULL) {
    std::auto_ptr<grdListenerModule> guardListenerModule(new grdListenerModule());
    guardListenerModule->setNotifier(m_commandNotifier);
    scheduler->addModule(guardListenerModule.get());
    m_modules.push_back(guardListenerModule.release());
  }

  //std::auto_ptr<scModule> guardJobWorker(new scJobWorkerModule());
  //res->addModule(guardJobWorker.get());
  //m_modules.push_back(guardJobWorker.release());

  //std::auto_ptr<scModule> guardShRes(new scSharedResourceModule());
  //res->addModule(guardShRes.get());
  //m_modules.push_back(guardShRes.release());
}

//-------------------------------------------------------------------------------
// grdYieldSignalForCompactSrv
//-------------------------------------------------------------------------------
grdYieldSignalForCompactSrv::grdYieldSignalForCompactSrv(grdCompactServer *server, uint interval): 
  scYieldSignal(SC_NULL, interval), 
  m_server(server) 
{
}

grdYieldSignalForCompactSrv::~grdYieldSignalForCompactSrv()
{
}

void grdYieldSignalForCompactSrv::invokeYield()
{
    assert(m_server != SC_NULL);
    if (m_interval > 0)
      m_server->runYieldBusy();
    else
      m_server->runYieldWait();
}

