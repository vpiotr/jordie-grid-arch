/////////////////////////////////////////////////////////////////////////////
// Name:        CoreModule.h
// Project:     scLib
// Purpose:     Always-required base module. Application-level.
//              Basic node management & utility functions.
// Author:      Piotr Likus
// Modified by:
// Created:     12/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _COREMODULE_H__
#define _COREMODULE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file CoreModule.hpp
///
/// File description

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// bost
#include <boost/shared_ptr.hpp>
#include "boost/ptr_container/ptr_map.hpp"

// dtp
#include "dtp/dnode_serializer.h"

// grd
#include "grd\core.h"
#include "grd\CommandParser.h"
#include "grd\GateFactory.h"
#include "grd\ModuleImpl.h"
#include "grd\details\SchedulerImpl.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
typedef boost::ptr_map<scString, scGateFactory> scGateFactoryColn;

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class scCommandParser;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scNoParamFunctor
// ----------------------------------------------------------------------------
/// Functor with no params
class scNoParamFunctor {
public:
  virtual void operator()() = 0;
};

// ----------------------------------------------------------------------------
// scStrParamFunctor
// ----------------------------------------------------------------------------
/// Functor with one param - string
class scStrParamFunctor {
public:
  virtual void operator()(const scString &a_value) = 0;
};

// ----------------------------------------------------------------------------
// scCoreModule
// ----------------------------------------------------------------------------
/// Module supporting "core.zzz" interface
/// List of supported commands:
/// + [core.]echo([text]) - works like "ping" - returns something or simply arguments
/// + get_stats - statistics, # of tasks(act/fin), # of messages(act/fin), # of modules, # of gates
/// + reg_node (source, target) - register node as, if source = empty - generate ID & return it
///   - params:
///    + source - source version
///    + target - target version
///    + "public=true" - alias is externally accessible,
///    + "direct_contact=true" - address can be contacted directly (fully resolve on advertise)
///    - "refresh_delay=30" - how long to wait to refresh the registration
///    - "ttl=40" - how long registration should be valid
///    - "single=true" - if single, next registration will replace the old value instead of adding
///    - share_time=30000 - defines how long registration should be active
///      when "borrowed" from directory
/// + reg_node_at (exec_at_addr, source_name [,target_name]) - register node at specified node
/// + advertise - returns matching aliases
///   - input: role-name[, key] ("@master", "squeue/Work*")
///   - output: service list: address[, type, params]
///   - can be used to ask for nodes or nodes-with-services
/// + set_dispatcher (address) - set dispatcher address (node where message should be fwd if unk receiver)
/// + set_directory - set address where unknown address is forwarded with request "advertise"
/// + set_name (name) - set main node name
/// + create_node (class_name [,count [,name]]) - adds a new node(s) 
/// - set_alias (name,'command_line') - set alias for command
/// - get_alias (name) - returns description of alias
/// + set_var (name, value) - define internal variable
/// + run (script_file) - run script file
/// + run_cmd (cmd="command") - run command
/// - add_file_log (file_name) - add file output for log
/// + shutdown_node - close node
/// + restart_node - restart node
/// - import_env (var_name) - import environment variable
/// o export_env (var_name) - export environment variable
/// + sleep (time-ms) - sleep for specified milliseconds
/// - add_gate(input|output, protocol, extra-param-list) - adds gate to active scheduler for a given protocol
/// - forward(address, fwd_command, (fwd_params|fwd_params_json)) - send message to address
/// - set_option name,value
///   - changes option, possible options:
///     "show_processing_time" - true/false - shows how long message was processed
///     "log_messages" - true/false - logs all messages & results
class scCoreModule: public scModule {
public:
    // -- creation --
    scCoreModule();
    virtual ~scCoreModule();
    // -- module support --
    virtual int handleMessage(const scEnvelope &envelope, scResponse &response);
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scStringList supportedInterfaces() const;
    // -- properties --
    void setCommandParser(scCommandParser *parser);
    void setOnShutdown(scNoParamFunctor *functor);
    scNoParamFunctor *getOnShutdown();
    void setOnRestart(scNoParamFunctor *functor);
    scNoParamFunctor *getOnRestart();
    // -- execution --
    void registerGateFactory(const scString &protocol, scGateFactory *factory);
protected:
    scScheduler *checkScheduler();    
    void performRestart();
    void performShutdown();
    void checkCommandParser();
    scString genStats();
    // --- commands ---
    int handleCmdIfDiff(scMessage *message, scResponse &response);
    int handleCmdIfEqu(scMessage *message, scResponse &response);
    int handleCmdEcho(scMessage *message, scResponse &response);
    int handleCmdRun(scMessage *message, scResponse &response);
    int handleCmdRunCmd(scMessage *message, scResponse &response);
    int handleCmdSetDispatcher(scMessage *message, scResponse &response);
    int handleCmdSetDirectory(scMessage *message, scResponse &response);
    int handleCmdGetStats(scMessage *message, scResponse &response);
    int handleCmdShutdownNode(scMessage *message, scResponse &response);
    int handleCmdRestartNode(scMessage *message, scResponse &response);
    int handleCmdSetNodeName(scMessage *message, scResponse &response);
    int handleCmdSetVar(scMessage *message, scResponse &response);
    int handleCmdRegNode(scMessage *message, scResponse &response);
    int handleCmdRegNodeAt(scMessage *message, scResponse &response);
    int handleCmdImportEnv(scMessage *message, scResponse &response);
    int handleCmdFlushEvents(scMessage *message, scResponse &response);
    int handleCmdCreateNode(scMessage *message, scResponse &response);
    int handleCmdForward(const scEnvelope &envelope, scResponse &response);
    int handleCmdAdvertise(const scEnvelope &envelope, scResponse &response);
    int handleCmdSleep(scMessage *message, scResponse &response);
    int handleCmdAddGate(scMessage *message, scResponse &response);
    int handleCmdSetOption(scMessage *message, scResponse &response);
    int handleCmdRegMap(scMessage *message, scResponse &response);
    // supporting functions
    bool setOption(const scString &optionName, const scString &optionValue);
    void setOption(scSchedulerFeature option, bool newValue);
protected:
    scCommandParser *m_commandParser;
    scNoParamFunctor *m_onShutdown;
    scNoParamFunctor *m_onRestart;
    scGateFactoryColn m_gateFactoryColn;
    boost::shared_ptr<dtp::dnSerializer> m_serializer;
};


#endif // _COREMODULE_H__