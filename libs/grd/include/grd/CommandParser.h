/////////////////////////////////////////////////////////////////////////////
// Name:        CommandParser.h
// Project:     scLib
// Purpose:     Converts string commands to "post" events 
//              performed by scheduler.
// Author:      Piotr Likus
// Modified by:
// Created:     12/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _COMMANDPARSER_H__
#define _COMMANDPARSER_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file CommandParser.h
///
/// Command syntax:
/// #main_node_name# - variable, can be in any place, forced replace
/// command_line = [target_node]command[:command[:command...]]
/// target_node = '['node_address']'
/// command = [interface'.']int_command params|alias
/// alias = aliased command line
/// interface = name of interface, can be skipped
/// int_command = command to be performed
/// params = nil|param_list
/// param_list = param[,param...]
/// param = value|param_name=value
/// value = simple_value|quoted_value
/// param_name = alphanumeric started with letter
/// simple_value = anything without (") character
/// quoted_value = 'int_quoted_value'|"int_quoted_value"
/// int_quoted_value = HTML-like quoted value (&quote; &amp;) 
///
/// Example:
///   [#/A123/]list_nodes  

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/core.h"
#include "grd/Scheduler.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
static const char *SC_CMDPARS_VAR_PID = "_PID";
static const char *SC_CMDPARS_VAR_EXEC_DIR = "_EXEC_DIR";
static const char *SC_CMDPARS_VAR_EXEC_PATH = "_EXEC_PATH";
static const char *SC_CMDPARS_VAR_EXEC_FNAME = "_EXEC_FNAME";

#ifdef SC_LOG_ENABLED
#define SC_LOG_CMDPARS
#endif

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
typedef std::map<scString, scString> scCommandAliasList;
typedef std::map<scString, scString> scVarList;

class scCommandParser {
public:
    scCommandParser();
    virtual ~scCommandParser();
    void setAlias(const scString &a_aliasName, const scString &a_command);
    scString getAlias(const scString &a_aliasName);
    void setVar(const scString &a_varName, const scString &a_value);
    scString getVar(const scString &a_varName, const scString &defValue);
    scString getVar(const scString &a_varName);
    bool isVarDefined(const scString &a_varName);
    void parseCommand(const scString &a_cmd);
    void setScheduler(const scSchedulerIntf *scheduler);
    scSchedulerIntf *getScheduler();
    void setExecPath(const scString &path);      
protected:
    scSchedulerIntf *checkScheduler();    
    void throwSyntaxError(const scString &text, int a_pos, int a_char) const;
    bool isSpecial(int c);
    bool isAscii(int c);
    void replaceVars(scString &text);
    void handleSetVar(scDataNode &params);
    void handleFlushEvents(); 
    void initVarList();
    scString calcExecPath();
    scString calcExecDir();
    scString calcExecFName();
    void updateExecVars();
protected:
    scSchedulerIntf *m_scheduler;      
    scCommandAliasList m_aliasList;
    scVarList m_varList;
    scString m_execPath;
};

#endif // _COMMANDPARSER_H__