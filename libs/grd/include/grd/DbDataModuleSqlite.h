/////////////////////////////////////////////////////////////////////////////
// Name:        DbDataModuleSqlite.h
// Project:     grdLib
// Purpose:     Sql processing module - for remote SQL server support.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDDBDATAMODSQLITE_H__
#define _GRDDBDATAMODSQLITE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file DbDataModuleSqlite.h
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
#include "sc/db/DbSqlite.h"

// grd
#include "grd/core.h"
#include "grd/DbDataModule.h"
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
class grdDbDataModuleSqlite: public grdDbDataModule {
public:
    grdDbDataModuleSqlite() {}
    virtual ~grdDbDataModuleSqlite() {}
    virtual void prepareDb() {
        m_db.reset(new scDbSqlite());
        dynamic_cast<scDbSqlite *>(m_db.get())->connect(m_dbPath);
    }
    virtual void dispose() {
        if (m_db.get())
            m_db->disconnect();
        grdDbDataModule::dispose();
    }
protected:
    virtual void decodeFunctions(const scDataNode &params, scDataNode &outParams);
};

#endif // _GRDDBDATAMODSQLITE_H__