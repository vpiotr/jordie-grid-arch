/////////////////////////////////////////////////////////////////////////////
// Name:        wxCliServGate.cpp
// Project:     scLib
// Purpose:     Implementation of gate using wxClient,wxServer,wxConnection
// Author:      Piotr Likus
// Modified by:
// Created:     29/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//sc
#include "grd/core.h"
#include "grd/wxCliServGate.h"

#ifdef SC_WXCS_USE_YAJL
#include "grd/EnvSerializerJsonYajl.h"
#else
#include "sc/EnvelopeSerializerJson.h"
#endif

#include "sc/utils.h"

#ifdef SC_TIMER_ENABLED
#include "perf/Timer.h"
#endif

#include "perf/Counter.h"

using namespace perf;

// ----------------------------------------------------------------------------
// scWxConnection
// ----------------------------------------------------------------------------
class scWxConnection: public wxConnection
{
public:
  scWxConnection(scWxCliServGate *gate, const scString &connectionId): wxConnection(m_buffer, WXSIZEOF(m_buffer)),
    m_gate(gate), m_connectionId(connectionId)
  {
#ifdef SC_WXCS_LOG_ENABLED
    scLog::addText("scWxConnection - cnstcr for "+connectionId);
#endif
  }
  virtual ~scWxConnection()
  {
    //scLog::addText("scWxConnection - dstcr for "+m_connectionId);
  }
  virtual bool OnExecute (const wxString& WXUNUSED(topic),
                          wxChar *data,
                          int size,
                          wxIPCFormat WXUNUSED(format))
  {
    handleMessage(data, size);
    return true;
  }
  virtual bool OnDisconnect(void)
  {
    handleDisconnect();
    return true;
  }
  static scString calcId(const wxString& sHost, const wxString& sService, const wxString& sTopic);
protected:
  void handleMessage(wxChar *data, int size);
  void handleDisconnect();
private:
  scWxCliServGate *m_gate;
  // character buffer
  wxChar m_buffer[SC_WXCS_MAX_MSG_SIZE];
  scString m_connectionId;
};

// ----------------------------------------------------------------------------
// scWxClient
// ----------------------------------------------------------------------------
scWxClient::scWxClient(scWxCliServGate *a_gate): wxClient()
{
  m_connection = SC_NULL;
  m_gate = a_gate;
}

scWxClient::~scWxClient()
{
  disconnect();
}

bool scWxClient::connect(const wxString& sHost, const wxString& sService, const wxString& sTopic)
{
  m_connectionId = scWxConnection::calcId(sHost, sService, sTopic);
  m_connection = MakeConnection(sHost, sService, sTopic);
  return m_connection != SC_NULL;
}

void scWxClient::disconnect()
{
    if (m_connection)
    {
        m_connection->Disconnect();
        delete m_connection;
        m_connection = SC_NULL;
    }
}

wxConnectionBase *scWxClient::OnMakeConnection()
{
  return new scWxConnection(m_gate, m_connectionId);
}


/// Decode message string->scEnvelope, m_gate->put()
void scWxConnection::handleMessage(wxChar *data, int size)
{
  scString str(data, size);
  m_gate->putEnvelopeStr(str);
}

void scWxConnection::handleDisconnect()
{
  m_gate->handleDisconnect(m_connectionId);
}

scString scWxConnection::calcId(const wxString& sHost, const wxString& sService, const wxString& sTopic)
{
  return sHost+"_"+sService+"_"+sTopic;
}


// ----------------------------------------------------------------------------
// scWxCliServGate
// ----------------------------------------------------------------------------
scWxCliServGate::scWxCliServGate(): scMessageGate()
{
#ifdef SC_WXCS_USE_YAJL
  m_serializer.reset(new scEnvSerializerJsonYajl());
#else
  m_serializer.reset(new scEnvelopeSerializerJson());
#endif
}

scWxCliServGate::~scWxCliServGate()
{
}

bool scWxCliServGate::supportsProtocol(const scString &protocol)
{
  return (protocol == SC_WXCS_PROTOCOL);
}

scString scWxCliServGate::calcServiceName(const scString &nodeName)
{
  return SC_WXCS_DEF_TOPIC_PFX + nodeName;
}

void scWxCliServGate::putEnvelopeStr(const scString &str)
{
  std::auto_ptr<scEnvelope> guard(new scEnvelope());
  Counter::inc("msg-size", str.length());
  m_serializer->convFromString(str, *guard.get());
  put(guard.release());
}

void scWxCliServGate::handleDisconnect(const scString &connectionId)
{ // empty here
}

void scWxCliServGate::deleteConnection(const scString &connectionId)
{ // empty here
}

void scWxCliServGate::setServiceName(const scString &name)
{
  m_serviceName = name;
}

scString scWxCliServGate::getServiceName() const
{
  return m_serviceName;
}

// ----------------------------------------------------------------------------
// scWxServer
// ----------------------------------------------------------------------------
class scWxServer: public wxServer
{
public:
  scWxServer(scWxCliServGate *gate): wxServer(), m_gate(gate) { nextId = 1; };
  ~scWxServer() {}
  virtual wxConnectionBase *OnAcceptConnection (const wxString& topic);
  void handleDisconnect(const scString &connectionId);
protected:
protected:
  scWxCliServGate *m_gate;
  uint nextId;
  scWxConnectionMapColn m_connections;
};

wxConnectionBase *scWxServer::OnAcceptConnection (const wxString& topic)
{
  if (topic != SC_WXCS_IPC_START)
  {
    return NULL;
  } else {
    scString newId = toString(nextId);
    ++nextId;
    scWxConnection *newItem = new scWxConnection(m_gate, newId);
    m_connections.insert(newId, newItem);
    return newItem;
  }
}

void scWxServer::handleDisconnect(const scString &connectionId)
{
  scWxConnectionMapColn::iterator p;

  p = m_connections.find(connectionId);
  if(p != m_connections.end())
    m_connections.erase(p);
}

// ----------------------------------------------------------------------------
// scWxCliServGateIn
// ----------------------------------------------------------------------------
scWxCliServGateIn::scWxCliServGateIn(): scWxCliServGate()
{
  m_server = SC_NULL;
}

scWxCliServGateIn::~scWxCliServGateIn()
{
  delete m_server;
}

void scWxCliServGateIn::init()
{
  m_server = new scWxServer(this);
  scString name = calcServiceName(getServiceName());

  if (!m_server->Create(name)) {
      delete m_server;
      m_server = SC_NULL;
  }
}

int scWxCliServGateIn::run()
{ //empty
  return 0;
}

void scWxCliServGateIn::handleDisconnect(const scString &connectionId)
{
  if (m_server != SC_NULL)
    dynamic_cast<scWxServer *>(m_server)->handleDisconnect(connectionId);
}

// ----------------------------------------------------------------------------
// scWxCliServGateOut
// ----------------------------------------------------------------------------
scWxCliServGateOut::scWxCliServGateOut() : scWxCliServGate()
{
}

scWxCliServGateOut::~scWxCliServGateOut()
{
  m_connections.clear();
}

int scWxCliServGateOut::run()
{
  int res = 0;
  std::auto_ptr<scEnvelope> envelopeGuard;
  scString nodeName;
  scMessageAddress target;

  // input
  while(!empty())
  {
    envelopeGuard.reset(get());
    res++;
    try {
      transmitEnvelope(envelopeGuard.get());
    }
    catch (scError &e) {
      handleTransmitError(*envelopeGuard.get(), e);
    }
  } // while

  return res;
}

void scWxCliServGateOut::transmitEnvelope(scEnvelope *envelope)
{
  scString dataStr;
  scString host;
  scString serviceName;
  scString connectionId;

  m_serializer->convToString(*envelope, dataStr);
  serviceName = calcServiceName(envelope->getReceiver().getNode());
  host = envelope->getReceiver().getHost();

  connectionId = scWxConnection::calcId(host,serviceName,SC_WXCS_IPC_START);

  wxConnectionBase *item = findConnection(connectionId);
  if (item == SC_NULL)
  {
    std::auto_ptr<scWxClient> connGuard;
    connGuard.reset(new scWxClient(this));

  Counter::inc("msg-total");
  Counter::inc("msg-size", dataStr.length());

#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-connect-wxcs");
#endif
    connGuard.get()->connect(host, serviceName, SC_WXCS_IPC_START);
#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-connect-wxcs");
  Timer::stop("msg-total");
#endif
    addConnection(connectionId, connGuard.get());
    item = connGuard.release()->getConnection();
  }

  if (item) {
     if (dataStr.length() >= SC_WXCS_MAX_MSG_SIZE)
       throw scError("SCWX message too long ("+toString(dataStr.length())+")");
     wxChar data[ SC_WXCS_MAX_MSG_SIZE];
     wxStrncpy(data, dataStr.c_str(), dataStr.length()+1);

#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-execute-wxcs");
#endif
     bool status = item->Execute (data, (dataStr.length()+1)*sizeof(wxChar));
#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-execute-wxcs");
  Timer::stop("msg-total");
#endif
     if (!status)
       throw scError("SCWX gate.execute failed");
  }
  else
    throw scError("SCWX gate.connect failed");
}

bool scWxCliServGateOut::getOwnAddress(const scString &protocol, scMessageAddress &output)
{
  bool res = false;
  if (protocol == SC_WXCS_PROTOCOL)
  {
    output.clear();
    output.setProtocol(protocol);
    output.setNode(getServiceName());
    res = true;
  }
  return res;
}

void scWxCliServGateOut::handleDisconnect(const scString &connectionId)
{
  deleteConnection(connectionId);
}

void scWxCliServGateOut::deleteConnection(const scString &connectionId)
{
  scWxClientMapColn::iterator p;

  p = m_connections.find(connectionId);
  if(p != m_connections.end())
    m_connections.erase(p);
}

wxConnectionBase *scWxCliServGateOut::findConnection(const scString &connectionId)
{
  scWxClientMapColn::iterator p;

  p = m_connections.find(connectionId);
  if(p != m_connections.end())
//    return const_cast<wxConnectionBase *>(&(*(p->second)));
    return (p->second)->getConnection();
  else
    return SC_NULL;
}

void scWxCliServGateOut::addConnection(const scString &connectionId, scWxClient *item)
{
  m_connections.insert(const_cast<scString &>(connectionId), item);
}


