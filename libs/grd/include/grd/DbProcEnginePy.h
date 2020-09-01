/////////////////////////////////////////////////////////////////////////////
// Name:        DBProcEnginePy.h
// Project:     grdLib
// Purpose:     Database procedure engine - using python.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDDBPRCENGPY_H__
#define _GRDDBPRCENGPY_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file DBProcEnginePy.h
///
/// Python-based DB proc engine.

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "pycpp/pyintf.h"

#include "grd/DbProcEngine.h"

// ----------------------------------------------------------------------------
// Classes
// ----------------------------------------------------------------------------
class grdDbProcEnginePy: public grdDbProcEngineIntf {
public:
    grdDbProcEnginePy(const scString &rootPath, const scDataNode &context);
    virtual ~grdDbProcEnginePy();
    virtual uint procExec(const scString &procName, const scString &procPath, const scDataNode &params);
    virtual uint procSelect(const scString &procName, const scString &procPath, const scDataNode &params, scDataNode &output);
    virtual bool procExists(const scString &procName, const scString &procPath);
protected:
    void initEngine();
    void disposeEngine();
    scString buildScriptPath(const scString &procName, const scString &procPath);
protected:
    scString m_rootPath;
    scDataNode m_context;
    std::auto_ptr<pycProcEngine> m_procEngine;
    pycProcOutput *m_procOutput;
};

#endif // _GRDDBPRCENGPY_H__