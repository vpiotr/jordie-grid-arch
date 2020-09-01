/////////////////////////////////////////////////////////////////////////////
// Name:        ServiceAppForGrd.cpp
// Project:     grdLib
// Purpose:     Service application base class using compact server.
// Author:      Piotr Likus
// Modified by:
// Created:     03/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/ServiceAppForGrd.h"
#include "sc/proc/process.h"
#include "perf/Log.h"

using namespace perf;

// ----------------------------------------------------------------------------
// grdServiceAppForGrd
// ----------------------------------------------------------------------------
void grdServiceAppForGrd::init()
{
  scServiceApp::init();   
  initServer(m_argc, m_argv);
}

void grdServiceAppForGrd::runServer()
{
    bool bContinue;
    do {
        bContinue = performStep(); 
        //sleepProcess(10);
    } while (bContinue);
}

bool grdServiceAppForGrd::performStep()
{
    m_server->runStep();
    return m_server->needsRun();
}

void grdServiceAppForGrd::stop()
{
    m_server->requestStop();
    m_server->waitForStop();
}

void grdServiceAppForGrd::runAsConsole()
{
    init();
    Log::addInfo("Server started, press Ctrl+C to quit...");
    runServer();
}

void grdServiceAppForGrd::initServer(int argc, char* argv[])
{
    m_server.reset(newServer());
    m_server->init();
    m_server->parseCommandLine(argc, argv);
}

void grdServiceAppForGrd::run()
{
    runAsConsole();
}
