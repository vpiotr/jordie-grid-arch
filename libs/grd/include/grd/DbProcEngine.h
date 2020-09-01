/////////////////////////////////////////////////////////////////////////////
// Name:        DBProcEngine.h
// Project:     grdLib
// Purpose:     Database procedure engine - interface.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDDBPRCENG_H__
#define _GRDDBPRCENG_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file DBProcEngine.h
///
/// DB proc engine interface.

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"

// ----------------------------------------------------------------------------
// Classes
// ----------------------------------------------------------------------------
class grdDbProcEngineIntf {
public:
    grdDbProcEngineIntf() {}
    virtual ~grdDbProcEngineIntf() {}
    virtual uint procExec(const scString &procName, const scString &procPath, const scDataNode &params) = 0;
    virtual uint procSelect(const scString &procName, const scString &procPath, const scDataNode &params, scDataNode &output) = 0;
    virtual bool procExists(const scString &procName, const scString &procPath) = 0;
};

#endif // _GRDDBPRCENG_H__