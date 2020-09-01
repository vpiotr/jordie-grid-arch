/////////////////////////////////////////////////////////////////////////////
// Name:        W32ServiceAppForGrd.h
// Project:     grdLib
// Purpose:     Service app with Win32 features: clean shutdown in console 
//              mode and Windows service support.
// Author:      Piotr Likus
// Modified by:
// Created:     03/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/W32ServiceAppForGrd.h"

#include "perf\Log.h"

//#include "sc/W32Service.h"
#include "sc/proc/W32Service.h"

using namespace perf;

// ----------------------------------------------------------------------------
// private declarations
// ----------------------------------------------------------------------------
class W32ConsoleShutdownListener: public scListener {
public:
    W32ConsoleShutdownListener(grdServiceAppForGrd *app): scListener(), m_app(app) {
        assert(app != SC_NULL);
    }
    virtual ~W32ConsoleShutdownListener() {}
    virtual void handle(const scString &eventName, scDataNode *eventData) {
        Log::addInfo(scString("Shutdown started... reason: [") + eventName+"]");
        m_app->stop();
    }
protected:
    grdServiceAppForGrd *m_app;
};


// ----------------------------------------------------------------------------
// grdW32ServiceAppForGrd
// ----------------------------------------------------------------------------
void grdW32ServiceAppForGrd::run()
{
    if (getRunMode() == sarmService)
        runAsService();
    else
        runAsConsole();
}

void grdW32ServiceAppForGrd::runAsService()
{
    std::auto_ptr<scW32ServiceManager> service(newServiceManager());
    scW32ServiceManager::run();
}

void grdW32ServiceAppForGrd::initServer(int argc, char* argv[])
{
    grdServiceAppForGrd::initServer(argc, argv);
    if (getRunMode() == sarmConsole) {
        initForConsoleMode();
    }
}

void grdW32ServiceAppForGrd::initForConsoleMode()
{
    m_shutdownMonitor.reset(new W32ConsoleShutdownMonitor());
    m_shutdownNotifier.reset(new scNotifier());
    m_shutdownListener.reset(new W32ConsoleShutdownListener(this));
    m_shutdownNotifier->addListener("osnote.close_exec", m_shutdownListener.get());
    m_shutdownNotifier->addListener("osnote.shutdown_exec", m_shutdownListener.get());
    m_shutdownNotifier->addListener("osnote.logoff_exec", m_shutdownListener.get());

    m_shutdownMonitor->setNotifier(m_shutdownNotifier.get());

    m_shutdownMonitor->init();
}

bool grdW32ServiceAppForGrd::performStep()
{
    bool res = grdServiceAppForGrd::performStep();
    stepPerformed();
    return res;
}

void grdW32ServiceAppForGrd::stepPerformed()
{
   if (m_shutdownMonitor.get() != SC_NULL)
     m_shutdownMonitor->unload();
}

