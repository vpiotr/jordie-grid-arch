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

#ifndef _W32SERAPPGRD_H__
#define _W32SERAPPGRD_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
/// File description

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/ServiceAppForGrd.h"

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


// service application with win32 console & service support
class grdW32ServiceAppForGrd: public grdServiceAppForGrd {
public:
    grdW32ServiceAppForGrd(): grdServiceAppForGrd() {}
    virtual ~grdW32ServiceAppForGrd() {}
    virtual void run();
protected:
    void initServer(int argc, char* argv[]);
    void initForConsoleMode();
    virtual scW32ServiceManager *newServiceManager() = 0;
    virtual bool performStep();
    virtual void stepPerformed();
    virtual void runAsService();
protected:
    std::auto_ptr<W32ConsoleShutdownMonitor> m_shutdownMonitor;
    std::auto_ptr<scNotifier> m_shutdownNotifier;
    std::auto_ptr<scListener> m_shutdownListener;
};


#endif // _W32SERAPPGRD_H__