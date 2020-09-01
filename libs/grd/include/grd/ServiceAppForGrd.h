/////////////////////////////////////////////////////////////////////////////
// Name:        ServiceAppForGrd.h
// Project:     grdLib
// Purpose:     Service application base class using compact server.
//              Includes shutdown handling in Win32 console mode & supports 
//              Win32 services.
// Author:      Piotr Likus
// Modified by:
// Created:     03/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _GRDSERVICEAPPGRD_H__
#define _GRDSERVICEAPPGRD_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
/// File description

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/ServiceApp.h"
#include "grd/CompactServer.h"
#include "sc/proc/W32ServiceAppManager.h"
#include "sc/proc/W32Console.h"

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

// base app for grd-like daemon application
class grdServiceAppForGrd: public scServiceApp {
public:
    grdServiceAppForGrd(): scServiceApp() {}
    virtual ~grdServiceAppForGrd() {}
    virtual void stop();
    virtual void run();
protected:
    virtual void init();
    virtual void initServer(int argc, char* argv[]);
    virtual void runServer();
    virtual void runAsConsole();
    virtual bool performStep();
    virtual grdCompactServer *newServer() = 0;
protected:
    std::auto_ptr<grdCompactServer> m_server;
};

#endif // _GRDSERVICEAPPGRD_H__