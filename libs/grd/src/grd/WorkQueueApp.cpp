/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueApp.cpp
// Project:     grdLib
// Purpose:     Universal work queue application object
//              Ready to be used as container for worker & client.
// Author:      Piotr Likus
// Modified by:
// Created:     26/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// perf
#include "perf/Log.h"
#include "perf/Counter.h"

// grd
#include "grd/WorkQueueApp.h"
#include "grd/CompactServer.h"
#include "grd/WorkQueueClientImpl.h"
#include "grd/details/SchedulerImpl.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace perf;

const uint DEF_YIELD_DELAY = 500;

//-------------------------------------------------------------------------------
// Private classes
//-------------------------------------------------------------------------------
class scWqWorkerEntry;

typedef boost::ptr_vector<scWqWorkerEntry> scWqWorkerColn;

class scWqAppBodyImpl: public scWqAppBody {
public:
// construct
  scWqAppBodyImpl(): scWqAppBody(), m_prepared(false), m_initDone(false) {}
  virtual ~scWqAppBodyImpl() {}
// properties
  void setCliArgs(const scDataNode &args);
  void addTask(scTask *task);
  void addWorker(scWqWorker *worker);
  virtual scWqServerProxy *prepareServerProxy();
  virtual scSignal *prepareYieldSignal(bool withDelay);
  void setStopOnIdle(bool value);  
// run
  void init(); 
  void run();
  void yieldBusy();
  void yieldWait();
protected:
  void checkInitDone();
  void checkPrepared();
  bool isPrepared();
  void prepare();
  virtual void initServer();
  //virtual scModule *createWorkHandlerModule();
  //virtual void registerWorkHandler(const scString &requestCommand, scWqWorkHandler *handler, scModule &workerModule);
  virtual scScheduler *getScheduler();
  virtual scNotifier *prepareNotifier();
  const scString getWorkTarget();
  grdCompactServer *prepareCompactServer();
protected:
  bool m_prepared;
  bool m_initDone;
  std::auto_ptr<grdCompactServer> m_compactServer;
  scDataNode m_cliArgs;
  std::auto_ptr<scWqServerProxy> m_serverProxy;
  std::auto_ptr<scNotifier> m_notifier; 
  std::auto_ptr<scSignal> m_yieldSignalDelay;
  std::auto_ptr<scSignal> m_yieldSignalNoDelay;
  std::auto_ptr<scListener> m_yieldListener;
  scWqWorkerColn m_workers;
};

class scWqYieldListener: public scListener {
public:
  scWqYieldListener(scWqAppBodyImpl *owner): m_owner(owner) {}
  virtual void handle(const scString &eventName, scDataNode *eventData) {
    uint interval;
    if (eventData != SC_NULL)
      interval = 1;
    else
      interval = 0;

    if (interval > 0)
      m_owner->yieldBusy();
    else
      m_owner->yieldWait();
	  
    //Counter::inc("grd-yield-lsn");
  }
protected:
  scWqAppBodyImpl *m_owner;  
};

class scWqWorkerEntry {
public:
  scWqWorkerEntry(scWqWorker *worker, scListener *listener): m_worker(worker), m_listener(listener) {}
protected:
  std::auto_ptr<scWqWorker> m_worker;
  std::auto_ptr<scListener> m_listener;
};

struct LastTimeMarker {
  LastTimeMarker(cpu_ticks *outputVar) { m_outputVar = outputVar; }
  ~LastTimeMarker() { 
     *m_outputVar = cpu_time_ms(); 
     // make sure 0 is not stored (special value for locks)
     if (*m_outputVar == 0)
       *m_outputVar = 1;
  }
protected:
  cpu_ticks *m_outputVar;  
};

//-------------------------------------------------------------------------------
// scWqAppBodyImpl
//-------------------------------------------------------------------------------
scWqServerProxy *scWqAppBodyImpl::prepareServerProxy()
{
  if (m_serverProxy.get() == SC_NULL)
    m_serverProxy.reset(new scWqServerProxyImpl(getScheduler(), prepareYieldSignal(false), getWorkTarget()));
  return m_serverProxy.get();  
}

grdCompactServer *scWqAppBodyImpl::prepareCompactServer()
{
  if (m_compactServer.get() == SC_NULL)
    m_compactServer.reset(new grdCompactServer());
  return m_compactServer.get();  
}

scScheduler *scWqAppBodyImpl::getScheduler()
{
  return dynamic_cast<scScheduler *>(prepareCompactServer()->getScheduler());  
}

scSignal *scWqAppBodyImpl::prepareYieldSignal(bool withDelay)
{
  if (m_yieldSignalDelay.get() == SC_NULL) {
    m_yieldSignalDelay.reset(new scWqYieldSignal(prepareNotifier()));
    dynamic_cast<scWqYieldSignal *>(m_yieldSignalDelay.get())->setInterval(DEF_YIELD_DELAY);
    m_yieldListener.reset(new scWqYieldListener(this));
    prepareNotifier()->addListener("yield", m_yieldListener.get());
  }  

  if (m_yieldSignalNoDelay.get() == SC_NULL) {
    m_yieldSignalNoDelay.reset(new scWqYieldSignal(prepareNotifier()));
  }  

  if (withDelay)
    return m_yieldSignalDelay.get();
  else
    return m_yieldSignalNoDelay.get();
}

scNotifier *scWqAppBodyImpl::prepareNotifier()
{
  if (m_notifier.get() == SC_NULL)
    m_notifier.reset(new scNotifier);
  return m_notifier.get();  
}

const scString scWqAppBodyImpl::getWorkTarget()
{
  //return scString("@worker");
  return scString("");
}

void scWqAppBodyImpl::addTask(scTask *task)
{
  checkInitDone();
  getScheduler()->addTask(task);
}

void scWqAppBodyImpl::addWorker(scWqWorker *worker)
{ 
  std::auto_ptr<scWqWorker> workerGuard(worker);
  std::auto_ptr<scWqWorkerListener> listerGuard(new scWqWorkerListener(worker));
  
  scStringList cmdList;
  scMessage commandMsg;
  
  worker->getSupportedCommands(cmdList);
  
  //for(uint i=0, epos = cmdList.size(); i != epos; i++)
  for(scStringList::iterator it = cmdList.begin(), epos = cmdList.end(); it != epos; ++it)
  {
    //commandMsg.setCommand(cmdList[i]);
    commandMsg.setCommand(*it);
    //m_notifier->addListener(cmdList[i], listerGuard.get());
    m_notifier->addListener(*it, listerGuard.get());
    m_compactServer->addInterfaceToObserved(commandMsg.getInterface());
  }  
    
  if (!cmdList.empty()) {  
    m_workers.push_back(new scWqWorkerEntry(workerGuard.release(), listerGuard.release()));
  }  

  scWqModalWorker *mworker = dynamic_cast<scWqModalWorker *>(worker);
  if (mworker != SC_NULL)
    mworker->setYieldSignal(prepareYieldSignal(true));
}

void scWqAppBodyImpl::setCliArgs(const scDataNode &args)
{
  scString script;

  if (args.hasChild("ri"))
    script = args.getString("ri");
  else if (args.hasChild("rs"))
    script = args.getString("rs");
  else
    script = "{none}";

  Log::addDebug(scString("script-body: ") + script);
  m_cliArgs = args;
}

void scWqAppBodyImpl::setStopOnIdle(bool value)
{
  prepareCompactServer()->setStopOnIdle(value);
}

void scWqAppBodyImpl::initServer()
{
  m_compactServer->init();
  m_compactServer->setCommandNotifier(prepareNotifier());
}

// create all internal objects
void scWqAppBodyImpl::init()
{
  initServer();
  m_initDone = true;
}

void scWqAppBodyImpl::checkPrepared()
{
  checkInitDone();
  if (!isPrepared())
    prepare();
}

bool scWqAppBodyImpl::isPrepared()
{
  return m_prepared;
}

void scWqAppBodyImpl::prepare()
{
  m_compactServer->parseCommandLine(m_cliArgs);
  m_prepared = true;
}

void scWqAppBodyImpl::checkInitDone()
{ 
  if (!m_initDone)
    init();
}

void scWqAppBodyImpl::run()
{
  checkPrepared();
  m_compactServer->run();
}

void scWqAppBodyImpl::yieldBusy()
{
  checkPrepared();
  m_compactServer->runYieldBusy();
}

void scWqAppBodyImpl::yieldWait()
{
  checkPrepared();
  m_compactServer->runYieldWait();
}

//-------------------------------------------------------------------------------
// scWqApp
//-------------------------------------------------------------------------------
scWqApp::scWqApp()
{
}

scWqApp::~scWqApp()
{
}

scWqAppBody *scWqApp::newBody()
{
  return new scWqAppBodyImpl;
}

scWqAppBody *scWqApp::prepareBody()
{
  if (m_body.get() == SC_NULL)
    m_body.reset(newBody());
  return m_body.get();  
}

void scWqApp::setCliArgs(const scDataNode &args)
{
  static_cast<scWqAppBodyImpl *>(prepareBody())->setCliArgs(args);
}

void scWqApp::setStopOnIdle(bool value)
{
  static_cast<scWqAppBodyImpl *>(prepareBody())->setStopOnIdle(value);
}

void scWqApp::addTask(scTask *task)
{
  static_cast<scWqAppBodyImpl *>(prepareBody())->addTask(task);
}

void scWqApp::addWorker(scWqWorker *worker)
{
  static_cast<scWqAppBodyImpl *>(prepareBody())->addWorker(worker);
}

scWqServerProxy *scWqApp::prepareServerProxy() {
  return 
    static_cast<scWqAppBodyImpl *>(prepareBody())->prepareServerProxy();
}  

scSignal *scWqApp::prepareYieldSignal(bool withDelay)
{
  return 
    static_cast<scWqAppBodyImpl *>(prepareBody())->prepareYieldSignal(withDelay);
}

void scWqApp::init()
{
  static_cast<scWqAppBodyImpl *>(prepareBody())->init();
}

void scWqApp::run()
{
  static_cast<scWqAppBodyImpl *>(prepareBody())->run();
}


//----------------------------------------------------------------
// scWqTaskContainerForModalTask
//----------------------------------------------------------------

int scWqTaskContainerForModalTask::intRun() 
{
  assert(m_ownedTask.get() != SC_NULL);
  bool isError = true;

  try {
    m_ownedTask->run();
    isError = false;
  } 
  catch(scError &excp) {
    scString msg = scString("Exception (scError): ") + excp.what()+", code: "+toString(excp.getErrorCode())+", details: "+excp.getDetails();
    Log::addError(msg);
  }
  catch(const std::exception& e) {
    scString msg = scString("Exception (std): ") + e.what();
    Log::addError(msg);
  }
  catch(...) {
    scString msg = "Exception (unknown type)";
    Log::addError(msg);
  }
  
  if (isError)
    Log::addError("Modal task failed");

  requestStop();
  return 0;
}  


//----------------------------------------------------------------
// scWqYieldSignal
//----------------------------------------------------------------
scWqYieldSignal::scWqYieldSignal(scNotifier *notifier, uint interval): scSignal(notifier), m_interval(interval) 
{
  m_lastYieldEnd = cpu_time_ms(); 
  // add 1ms because 0 is a special value (locks execution)
  if (m_lastYieldEnd == 0)
    m_lastYieldEnd++;
}

scWqYieldSignal::~scWqYieldSignal() 
{
}
 
void scWqYieldSignal::setInterval(uint value)
{
  m_interval = value;
}

void scWqYieldSignal::execute() 
{ 
  //Counter::inc("grd-yield-signal-exec");

  if (m_interval == 0) {
    invokeYield();
    return;
  }

  if (m_lastYieldEnd == 0)
    return;

  //Counter::inc("grd-yield-signal-chk");

  if (is_cpu_time_elapsed_ms(m_lastYieldEnd, m_interval)) {
    m_lastYieldEnd = 0; // block calls until return
    LastTimeMarker marker(&m_lastYieldEnd);  
    invokeYield();
  }
}

void scWqYieldSignal::invokeYield() 
{
  //Counter::inc("grd-yield-signal-invoke");
  
  if (m_interval > 0) {
    scDataNode intervalVal(m_interval);
    m_notifier->notify("yield", &intervalVal); 
  } else {
    m_notifier->notify("yield"); 
  }
}
