/////////////////////////////////////////////////////////////////////////////
// Name:        DBProcEnginePy.cpp
// Project:     grdLib
// Purpose:     Database procedure engine - using python.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// boost 
#include <boost/filesystem/operations.hpp>

// dtp
#include "dtp/dnode_serializer.h"

#include "base/file_utils.h"
#include "perf/Log.h"

// grd
#include "grd/DBProcEnginePy.h"

using namespace dtp;
using namespace perf;

//-------------------------
// local classes
//-------------------------

// engine for execution of Python procedures on server using selected db as context
class pycProcOutputForLog: public pycProcOutput {
public:
    virtual void intAddText(const std::string &aText, bool isError) {
        if (isError)
            Log::addError("Python: "+aText);
        else
            Log::addInfo("Python: "+aText);
    }
};

// ----------------------------------------------------------------------------
// grdDbProcEnginePy
// ----------------------------------------------------------------------------
grdDbProcEnginePy::grdDbProcEnginePy(const scString &rootPath, const scDataNode &context):
  m_rootPath(rootPath), m_context(context)
{
    initEngine();
}

grdDbProcEnginePy::~grdDbProcEnginePy()
{
    disposeEngine();
}

void grdDbProcEnginePy::initEngine()
{
    m_procOutput = new pycProcOutputForLog;
    m_procEngine.reset(new pycProcEngine);
}

void grdDbProcEnginePy::disposeEngine()
{
    m_procEngine.reset();
    delete m_procOutput;
    m_procOutput = SC_NULL;
}

scString grdDbProcEnginePy::buildScriptPath(const scString &procName, const scString &procPath)
{
    scString procDir;
    scString scriptFName;
    boost::filesystem::path res;

    if (procPath.empty()) {
        // use provided name and root dir to build access file path
        procDir = m_rootPath;
        scriptFName = procName+".py";
        res = boost::filesystem::path(procDir);
        res/=boost::filesystem::path(scriptFName);
    }
    else {
        // use provided procedure path
        res = boost::filesystem::path(procPath);
    }

    //res.replace_extension("");
    scString fname = res.stem().string();
    res.remove_filename();
    res /= fname;

    //return res.native_file_string();
	return res.string();
}

uint grdDbProcEnginePy::procExec(const scString &procName, const scString &procPath, const scDataNode &params)
{
    initEngine();

    Log::addDebug(scString("proc name=[")+procName+"], in-path=["+procPath+"]");

    scString filePath = buildScriptPath(procName, procPath);

    Log::addDebug(scString("proc name=[")+procName+"], path=["+filePath+"]");
    std::auto_ptr<pycProc> procGuard(m_procEngine->newProcFromFile(filePath.c_str(), "main"));

    dnSerializer serializer;
    scString paramsTxt;
    //scString resultTxt;
    std::string paramsTxtStd, resultTxtStd;
    scDataNode resultData;

    serializer.convToString(params, paramsTxt);

    paramsTxtStd = paramsTxt.c_str();

    procGuard->execute(paramsTxtStd, resultTxtStd);
    uint res = procGuard->getLastError();

    disposeEngine();

    return res;
}

uint grdDbProcEnginePy::procSelect(const scString &procName, const scString &procPath, const scDataNode &params, scDataNode &output)
{
    initEngine();

    scString filePath = buildScriptPath(procName, procPath);
    std::auto_ptr<pycProc> procGuard(m_procEngine->newProcFromFile(filePath.c_str(), "main"));

    dnSerializer serializer;
    scString paramsTxt;
    //scString resultTxt;
    std::string paramsTxtStd, resultTxtStd;
    scDataNode resultData;

    output.clear();

    serializer.convToString(params, paramsTxt);

    paramsTxtStd = paramsTxt.c_str();

    uint res;
    procGuard->execute(paramsTxtStd, resultTxtStd);
    res = procGuard->getLastError();

    if (res == 0) {
        serializer.convFromString(resultTxtStd.c_str(), output);
    }

    disposeEngine();

    return res;
}

bool grdDbProcEnginePy::procExists(const scString &procName, const scString &procPath)
{
    initEngine();
    scString filePath = buildScriptPath(procName, procPath);

    namespace fs = boost::filesystem;
    fs::path p(filePath);
    //fs::path full_p = fs::complete(p);
    fs::path full_p = fs::system_complete(p);

    //return fileExists(full_p.native_file_string());
	return fileExists(full_p.string());
}

