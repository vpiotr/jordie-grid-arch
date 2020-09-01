/////////////////////////////////////////////////////////////////////////////
// Name:        DbDataModule.h
// Project:     grdLib
// Purpose:     Sql processing module - for remote SQL server support.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDDBDATAMOD_H__
#define _GRDDBDATAMOD_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file DbDataModule.h
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
// sc
#include "sc/dtypes.h"
#include "sc/db/DbBase.h"

// grd
#include "grd/core.h"
#include "grd/DbProcEngine.h"

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
// abstract layer for database engine, build on top of scDbBase
// implementes abstraction features like:
// - common functions (like current date-time)
// - procedure execution
// - limit
// - offset
// - meta list
// - feature list
// - database connection path parsing
class grdDbDataModule {
public:
    grdDbDataModule();
    virtual ~grdDbDataModule();
    virtual void setup(const scString &fullDbPath, const scString &dbPath, const scString &procPath, bool procEnabled, const scDataNode &procDefList);
    virtual void init();    // -- connect to database
    virtual void dispose(); // -- disconnect
    virtual uint rowsAffected();
    virtual uint sqlExec(const scString &query, const scDataNode &params);
    virtual void sqlSelect(const scString &query, const scDataNode &params, uint limit, ulong64 offset, scDataNode &output);
    virtual uint sqlExecBatch(const scString &queryBatch, const scDataNode &params);
    virtual uint procExec(const scString &procName, const scDataNode &params);
    virtual uint procSelect(const scString &procName, const scDataNode &params, scDataNode &output);
    virtual void readRows(const scString &objName, const scDataNode &columns, const scDataNode &filter, const scDataNode &order,
        uint limit, ulong64 offset, scDataNode &output);
    virtual void insertRows(const scString &objName, const scDataNode &values);
    virtual uint updateRows(const scString &objName, const scDataNode &filter, const scDataNode &values);
    virtual uint deleteRows(const scString &objName, const scDataNode &filter);
    virtual void startTrans();
    virtual void commit();
    virtual void rollback();
    void getMetaObjList(const scString &objType, scDataNode &result);
    bool getMetaObjExists(const scString &objType, const scString &objName);
    bool getEngineSupports(uint flags, uint domain);
    scString getEngineName();
    ulong64 getLastInsId();
protected:
    virtual void prepareDb() = 0;
    virtual void prepareProcEngine();
    scDbBase *checkDb();
    grdDbProcEngineIntf *checkProcEngine();
    virtual void decodeFunctions(const scDataNode &params, scDataNode &outParams);
    scString genSqlFilter(const scDataNode &filter);
    scString genSqlOrder(const scDataNode &order);
    virtual bool isLimitSupported();
    virtual bool isOffsetSupported();
    void getProcContext(scDataNode &output);
    scDbObjType decodeObjectType(const scString &objType);
    bool getProcDef(const scString &procName, scDataNode &output);
    scString getProcPath(const scString &procName);
protected:
    std::auto_ptr<scDbBase> m_db;
    std::auto_ptr<grdDbProcEngineIntf> m_procEngine;
    scString m_fullDbPath;
    scString m_dbPath;
    scString m_procPath;
    bool m_procEnabled;
    scDataNode m_procDefList;
};


#endif // _GRDDBDATAMOD_H__