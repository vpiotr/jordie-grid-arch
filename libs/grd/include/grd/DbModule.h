/////////////////////////////////////////////////////////////////////////////
// Name:        DbModule.h
// Project:     grdLib
// Purpose:     Sql processing module - for remote SQL server support.
// Author:      Piotr Likus
// Modified by:
// Created:     20/01/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDDBMODULE_H__
#define _GRDDBMODULE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file DbModule.h
///
/// Uses connection strings like:
///    <engine>:<connection-string>
///
/// Examples:
///    sqlite://test.db
///    sqlite:memory
///    postgresql:host=localhost port=5432 dbname=mary

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "sc/db/DbBase.h"
#include "grd/core.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
typedef std::list<scTask *> grdDbTaskPtrList;

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const scString DBENGINE_SQLITE = "sqlite";
const scString DB_PARAM_TP_FUNC = "func";
const scString DB_FUNC_CURR_DT  = "current_dt";
const scString DB_OTYPE_TABLE    = "T";
const scString DB_OTYPE_VIEW     = "V";
const scString DB_OTYPE_PROC     = "P";
const scString DB_OTYPE_INDEX    = "I";

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class grdDbModule: public scModule 
{
public:
    grdDbModule(); 
    virtual ~grdDbModule(); 
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scStringList supportedInterfaces() const;
    virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
    void clearTaskRef(scTask *ref);
    bool getProcDef(const scString &alias, scDataNode &output);
    void getProcDefList(scDataNode &output);
protected:
    bool isModuleMessage(const scString &coreCmd);
    int forwardMsgByCid(scMessage *message, scResponse &response);
    // -- commands - begin
    int handleCmdInit(scMessage *message, scResponse &response);
    int handleCmdDefineDb(scMessage *message, scResponse &response);
    int handleCmdDefineProc(scMessage *message, scResponse &response);
    int handleCmdOpen(scMessage *message, scResponse &response);
    int handleCmdClose(scMessage *message, scResponse &response);
    // -- commands - end
    void setup(ulong64 inactTimeout);
    bool performDefineDb(const scString &alias, const scString &dbPath, bool procEnabled, const scString &procDir);
    bool performDefineProc(const scString &alias, const scString &procPath);
    bool connectionExists(const scString &cid);
    scTask *findConnectionTask(const scString &cid);
    scTask *checkConnectionTask(const scString &cid);
    void clearConnLinks();
    bool isInactTimeout();
    scTask *prepareConnectionManager(scMessage *message, const scString &cid);
    ulong64 genNextCid();
protected:
    ulong64 m_inactTimeout;
    ulong64 m_nextCid;
    scDataNode m_dbDefs;
    scDataNode m_procDefs;
    grdDbTaskPtrList m_connections;
};

#endif // _GRDDBMODULE_H__