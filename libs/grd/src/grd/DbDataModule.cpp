/////////////////////////////////////////////////////////////////////////////
// Name:        DbDataModule.cpp
// Project:     grdLib
// Purpose:     Sql processing module - for remote SQL server support.
// Author:      Piotr Likus
// Modified by:
// Created:     17/03/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// boost
#include <boost/algorithm/string.hpp>

// local
#include "grd/DbModule.h"
#include "grd/DbDataModule.h"
#include "grd/DbProcEnginePy.h"

using namespace dtp;

// ----------------------------------------------------------------------------
// grdDbDataModule
// ----------------------------------------------------------------------------
grdDbDataModule::grdDbDataModule(): m_procEnabled(false)
{
}

grdDbDataModule::~grdDbDataModule()
{
}

void grdDbDataModule::setup(const scString &fullDbPath, const scString &dbPath, const scString &procPath, bool procEnabled, const scDataNode &procDefList)
{
    m_fullDbPath = fullDbPath;
    m_dbPath = dbPath;
    m_procPath = procPath;
    m_procEnabled = procEnabled;
    m_procDefList = procDefList;
}

void grdDbDataModule::init()
{
    prepareDb();
    prepareProcEngine();
}

void grdDbDataModule::dispose()
{
    if (m_db.get() != SC_NULL)
        m_db->disconnect();
}

scDbBase *grdDbDataModule::checkDb()
{
    scDbBase *res = m_db.get();
    if (res == SC_NULL)
        throw scError("Database not initialized");
    return res;
}

grdDbProcEngineIntf *grdDbDataModule::checkProcEngine()
{
    grdDbProcEngineIntf *res = m_procEngine.get();
    if (res == SC_NULL)
        throw scError("Proc engine not initialized");
    if (!m_procEnabled)
        throw scError("Proc engine not enabled");
    return res;
}

void grdDbDataModule::prepareProcEngine()
{
    scDataNode procContext(ict_parent);
    procContext.setElementSafe("db_path", scDataNode(m_fullDbPath));
    m_procEngine.reset(new grdDbProcEnginePy(m_procPath, procContext));
}

uint grdDbDataModule::rowsAffected()
{
    return checkDb()->rowsAffected();
}

uint grdDbDataModule::sqlExec(const scString &query, const scDataNode &params)
{
    scDataNode realParams;
    scDataNode *paramsPtr;

    if (!params.isNull()) {
      decodeFunctions(params, realParams);
      paramsPtr = &realParams;
    } else {
      paramsPtr = SC_NULL;
    }

    return checkDb()->execute(query, paramsPtr);
}

uint grdDbDataModule::sqlExecBatch(const scString &queryBatch, const scDataNode &params)
{
    scDataNode realParams;
    scDataNode *paramsPtr;

    if (!params.isNull()) {
      decodeFunctions(params, realParams);
      paramsPtr = &realParams;
    } else {
      paramsPtr = SC_NULL;
    }

    std::vector<scString> commands;
    boost::split(commands, queryBatch, boost::is_any_of(";"));

    uint res = 0;
    for(std::vector<scString>::const_iterator it = commands.begin(), epos = commands.end(); it != epos; ++it)
        res += checkDb()->execute(*it, paramsPtr);

    return res;
}

void grdDbDataModule::sqlSelect(const scString &query, const scDataNode &params, uint limit, ulong64 offset, scDataNode &output)
{
    scDataNode realParams;
    scDataNode *paramsPtr;

    if (!params.isNull()) {
      decodeFunctions(params, realParams);
      paramsPtr = &realParams;
    } else {
      paramsPtr = SC_NULL;
    }

    checkDb()->getRowsAsVector(query, paramsPtr, limit, offset, output);
}

void grdDbDataModule::getProcContext(scDataNode &output)
{
    output.setElementSafe("db_path", scDataNode(m_fullDbPath));
}

bool grdDbDataModule::getProcDef(const scString &procName, scDataNode &output)
{
    bool res = m_procDefList.hasChild(procName);
    if (res)
        m_procDefList.getElement(procName, output);
    else
        output.clear();
    return res;
}

uint grdDbDataModule::procExec(const scString &procName, const scDataNode &params)
{
    scDataNode procParams(ict_parent);
    getProcContext(procParams);
    procParams.setElementSafe("proc_params", params);

    scString procPath = getProcPath(procName);

    return checkProcEngine()->procExec(procName, procPath, procParams);
}

uint grdDbDataModule::procSelect(const scString &procName, const scDataNode &params, scDataNode &output)
{
    scDataNode procParams(ict_parent);
    getProcContext(procParams);
    procParams.setElementSafe("proc_params", params);

    scString procPath = getProcPath(procName);
    return checkProcEngine()->procSelect(procName, procPath, procParams, output);
}

scString grdDbDataModule::getProcPath(const scString &procName)
{
    scDataNode procDef;
    getProcDef(procName, procDef);

    bool hasDef = !procDef.isNull();

    if (hasDef)
        return procDef.getString("path", "");
    else
        return "";
}

void grdDbDataModule::decodeFunctions(const scDataNode &params, scDataNode &outParams)
{
    outParams = params;
}

// extract field names from filter
// source form: "field: value"
// result form: "field = {field}"
scString grdDbDataModule::genSqlFilter(const scDataNode &filter)
{
    scString res;
    scString fieldName;

    for(uint i=0, epos = filter.size(); i != epos; i++) {
        if (!res.empty()) {
            res += " and ";
        }
        fieldName = filter.getElementName(i);
        res += " (" + fieldName + " = {" + fieldName + "}) ";
    }

    return res;
}

// convert pairs of values to "order by" phrase
// each field has boolean assigned meaning "in ascending order"
// source form: "field: false"
// result form: "field DESC"
scString grdDbDataModule::genSqlOrder(const scDataNode &order)
{
    scString res;
    scString fieldName;

    for(uint i=0, epos = order.size(); i != epos; i++) {
        if (!res.empty()) {
            res += ", ";
        }
        fieldName = order.getElementName(i);
        res += fieldName;
        if (!order.getBool(i))
            res += " DESC";
    }

    return res;
}

void grdDbDataModule::readRows(const scString &objName, const scDataNode &columns, const scDataNode &filter, const scDataNode &order,
    uint limit, ulong64 offset,
    scDataNode &output)
{
    scString sqlText = "select {__columns} from {__table}";
    scDataNode tplValues(ict_parent);
    tplValues.setElementSafe("__table", scDataNode(objName));

    scString columnText;

    if (columns.empty()) {
        columnText = "*";
    } else {
        columnText = columns.implode(",");
    }

    tplValues.setElementSafe("__columns", scDataNode(columnText));

    if (!filter.empty())
    {
        sqlText += " where {__filter}";
        scString filterText = genSqlFilter(filter);
        tplValues.setElementSafe("__filter", scDataNode( filterText));
    }

    if (!order.empty()) {
        sqlText += " order by {__order}";
        scString orderText = genSqlOrder(order);
        tplValues.setElementSafe("__order", scDataNode( orderText));
    }

    ulong64 clientLimit = 0;
    ulong64 clientOffset = 0;

    if (isLimitSupported()) {
      if (limit > 0)
         sqlText += " limit " + toString(limit);
    } else {
      clientLimit = limit;
    }

    if (isOffsetSupported()) {
      if (offset > 0)
        sqlText += " offset " + toString(offset);
    } else {
      clientOffset = offset;
    }

    sqlText = fillTemplateValues(sqlText, tplValues);

    sqlSelect(sqlText, filter, clientLimit, 0, output);
}

bool grdDbDataModule::isLimitSupported()
{
    return true;
}

bool grdDbDataModule::isOffsetSupported()
{
    return true;
}

void grdDbDataModule::insertRows(const scString &objName, const scDataNode &values)
{
    scString sqlText = "insert into {__table}({__columns}) values({__values})";
    scDataNode tplValues(ict_parent);
    tplValues.setElementSafe("__table", scDataNode( objName));

    scString columnText, valueText;

    columnText = values.childNames().implode(",");

    for(uint i=0, epos = values.size(); i != epos; i++) {
        if (!valueText.empty()) {
            valueText += ",";
        }
        valueText += "{"+values.getElementName(i)+"}";
    }

    tplValues.setElementSafe("__columns", scDataNode( columnText));
    tplValues.setElementSafe("__values", scDataNode( valueText));

    sqlText = fillTemplateValues(sqlText, tplValues);

    sqlExec(sqlText, values);
}

uint grdDbDataModule::updateRows(const scString &objName, const scDataNode &filter, const scDataNode &values)
{
    scString sqlText = "update {__table} set {__values}";
    scDataNode tplValues(ict_parent);
    tplValues.setElementSafe("__table", scDataNode( objName));

    scString valueText, fieldName;

    for(uint i=0, epos = values.size(); i != epos; i++) {
        if (!valueText.empty()) {
            valueText += ",";
        }
        fieldName = values.getElementName(i);
        valueText += fieldName + " = {"+fieldName+"}";
    }

    tplValues.setElementSafe("__values", scDataNode( valueText));

    if (!filter.empty())
    {
        sqlText += " where {__filter}";
        scString filterText = genSqlFilter(filter);
        tplValues.setElementSafe("__filter", scDataNode( filterText));
    }

    sqlText = fillTemplateValues(sqlText, tplValues);

    scDataNode sqlParams = values;
    sqlParams.merge(filter);

    return sqlExec(sqlText, sqlParams);
}

uint grdDbDataModule::deleteRows(const scString &objName, const scDataNode &filter)
{
    scString sqlText = "delete from {__table}";
    scDataNode tplValues(ict_parent);
    tplValues.setElementSafe("__table", scDataNode( objName));

    if (!filter.empty())
    {
        sqlText += " where {__filter}";
        scString filterText = genSqlFilter(filter);
        tplValues.setElementSafe("__filter", scDataNode( filterText));
    }

    sqlText = fillTemplateValues(sqlText, tplValues);

    return sqlExec(sqlText, filter);
}

void grdDbDataModule::startTrans()
{
    checkDb()->startTrans();
}

void grdDbDataModule::commit()
{
    checkDb()->commit();
}

void grdDbDataModule::rollback()
{
    checkDb()->rollback();
}

scDbObjType grdDbDataModule::decodeObjectType(const scString &objType)
{
    scDbObjType res = dotUndef;

    if (objType == DB_OTYPE_TABLE)
        res = dotTable;
    else if (objType == DB_OTYPE_VIEW)
        res = dotView;
    else if (objType == DB_OTYPE_PROC)
        res = dotProcedure;
    else if (objType == DB_OTYPE_INDEX)
        res = dotIndex;

    return res;
}

void grdDbDataModule::getMetaObjList(const scString &objType, scDataNode &result)
{
}

bool grdDbDataModule::getMetaObjExists(const scString &objType, const scString &objName)
{
    scDbObjType oType = decodeObjectType(objType);
    if (oType == dotProcedure) {
        scString procPath = getProcPath(objName);
        return checkProcEngine()->procExists(objName, procPath);
    } else {
        return m_db->getMetaObjExists(oType, objName);
    }
}

bool grdDbDataModule::getEngineSupports(uint flags, uint domain)
{
    return checkDb()->isSupported(flags, domain);
}

scString grdDbDataModule::getEngineName()
{
    return checkDb()->getVersion();
}

ulong64 grdDbDataModule::getLastInsId()
{
    return checkDb()->getLastInsertedId();
}
