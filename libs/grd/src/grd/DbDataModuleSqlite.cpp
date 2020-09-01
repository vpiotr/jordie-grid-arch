/////////////////////////////////////////////////////////////////////////////
// Name:        DbDataModuleSqlite.cpp
// Project:     grdLib
// Purpose:     Sql processing module - for SQLite support.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/DbModule.h"
#include "grd/DbDataModuleSqlite.h"

// ----------------------------------------------------------------------------
// grdDbDataModuleSqlite
// ----------------------------------------------------------------------------
// Convert params in form:
//   func: true - function indicator
//   value: current_dt - function name
//   params: <params> - optional params
// to SQL engine-specific expression or calculated value
void grdDbDataModuleSqlite::decodeFunctions(const scDataNode &params, scDataNode &outParams)
{
    outParams = params;
    for(uint i=0, epos = outParams.size(); i != epos; i++)
    {
        if (outParams[i].getBool(DB_PARAM_TP_FUNC, false)) {
            scString funcName = outParams[i].getString("value", "");
            if (funcName == DB_FUNC_CURR_DT) {
                std::auto_ptr<scDataNode> funcNode(scDbBase::newExpr("", "datetime('now')"));
                outParams.setElementValue(i, *funcNode);
            } else {
                throw scError(scString("Unknown function name: [")+funcName+"]");
            }
        }
    }
}
