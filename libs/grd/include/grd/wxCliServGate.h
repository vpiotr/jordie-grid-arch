/////////////////////////////////////////////////////////////////////////////
// Name:        wxCliServGate.h
// Project:     scLib
// Purpose:     Gate that communicates using wxClient,wxServer,wxConnection
// Author:      Piotr Likus
// Modified by:
// Created:     29/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _WXCLISERVGATE_H__
#define _WXCLISERVGATE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file wxCliServGate.h
///
/// Gate uses protocol named "wxcs".
///   
// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
//wx
#include <wx/ipc.h>      // IPC support
//boost
#include "boost/ptr_container/ptr_map.hpp"
//sc
#include "grd/core.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class scWxClient;
class scWxConnection;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const scString SC_WXCS_DEF_TOPIC_PFX = "IPC_GATE_";
const scString SC_WXCS_IPC_START = "StartOther";
const int SC_WXCS_MAX_MSG_SIZE = 65536;
static const char *SC_WXCS_PROTOCOL = "wxcs";

//#define SC_WXCS_LOG_ENABLED  
#define SC_WXCS_USE_YAJL

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
typedef boost::ptr_map<scString,scWxClient> scWxClientMapColn; 
typedef boost::ptr_map<scString,scWxConnection> scWxConnectionMapColn; 

// ----------------------------------------------------------------------------
// scWxCliServGate
// ----------------------------------------------------------------------------
/// Base for gates using wxClient/wxServer
class scWxCliServGate: public scMessageGate {
public:
  scWxCliServGate();
  virtual ~scWxCliServGate();
  virtual bool supportsProtocol(const scString &protocol);
  virtual void putEnvelopeStr(const scString &str);
  virtual void handleDisconnect(const scString &connectionId);
  virtual void deleteConnection(const scString &connectionId);
  // --- properties ---
  void setServiceName(const scString &name);
  scString getServiceName() const;
protected:  
  scString calcServiceName(const scString &nodeName);
protected:
  std::auto_ptr<scEnvelopeSerializerBase> m_serializer; 
  scString m_serviceName;   
};

// ----------------------------------------------------------------------------
// scWxCliServGateIn
// ----------------------------------------------------------------------------
/// Input gate using wxClient/wxServer
class scWxCliServGateIn: public scWxCliServGate {
public:
  scWxCliServGateIn();
  virtual ~scWxCliServGateIn();
  virtual int run();
  void init();
protected:  
  virtual void handleDisconnect(const scString &connectionId);
protected:
  wxServerBase *m_server;  
};

// ----------------------------------------------------------------------------
// scWxCliServGateOut
// ----------------------------------------------------------------------------
/// Output gate using wxClient/wxServer
class scWxCliServGateOut: public scWxCliServGate {
public:
  scWxCliServGateOut();
  virtual ~scWxCliServGateOut();
  virtual int run();
  virtual bool getOwnAddress(const scString &protocol, scMessageAddress &output);
  virtual void deleteConnection(const scString &connectionId);
protected:
  void transmitEnvelope(scEnvelope *envelope);
  wxConnectionBase *findConnection(const scString &connectionId);  
  void addConnection(const scString &connectionId, scWxClient *item);
  virtual void handleDisconnect(const scString &connectionId);
protected:
  scWxClientMapColn m_connections;
};

// ----------------------------------------------------------------------------
// scWxClient
// ----------------------------------------------------------------------------
class scWxClient: public wxClient
{
public:
    scWxClient(scWxCliServGate *a_gate);
    ~scWxClient();
    bool connect(const wxString& sHost, const wxString& sService, const wxString& sTopic);
    void disconnect();
    wxConnectionBase *OnMakeConnection();
    bool isConnected() { return m_connection != NULL; };
    wxConnectionBase *getConnection() { return m_connection; };
protected:
    wxConnectionBase *m_connection;
    scWxCliServGate *m_gate;
    scString m_connectionId;
};

#endif // _WXCLISERVGATE_H__