/////////////////////////////////////////////////////////////////////////////
// Name:        ZeroMQGates.h
// Project:     grdLib
// Purpose:     ZeroMQ message gates
// Author:      Piotr Likus
// Modified by:
// Created:     10/04/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

// zmq
#include "zmq.hpp"

// perf
#include "perf/Timer.h"
#include "perf/Counter.h"
#include "perf/Log.h"

// grd
#include "grd/ZeroMQGates.h"

#include "grd/MessageConst.h"
#include "grd/EnvSerializerJsonYajl.h"
#include "grd/MessageGate.h"
#include "grd/Connection.h"
#include "grd/ConnectionPool.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace perf;

const scString SC_PROTOCOL_TCP = "tcp";
const scString SC_PROTOCOL_IPC = "ipc";
const scString SC_PROTOCOL_PGM = "pgm";
const uint SC_ZMQ_DEF_INACT_CONN_TIMEOUT = 30000;
const uint SC_ZMQ_MAX_MSG_SIZE = 65536;
const scString SC_ZMQ_TOPIC_SEP = "|";

//----------------------------------------------------------------------------------
// Local classes
//----------------------------------------------------------------------------------
class zmConnectionOut;

typedef boost::ptr_map<scString,zmConnectionOut> zmConnectionOutMap; 
typedef std::auto_ptr<zmq::socket_t> zmSocketGuard; 

class zmContext: public zmContextBase {
public:
  zmContext();
  virtual ~zmContext();
  zmq::context_t &getHandle();
  virtual void clear();
protected:
  std::auto_ptr<zmq::context_t> m_context;
};

class zmGate: public scMessageGate {
public:
  zmGate(zmContext *context);
  virtual ~zmGate();
  void setZmqProtocol(const scString &protocol);
  void setProtocol(const scString &protocol);
  void setAddress(const scString &address);
  void setInactTimeout(uint msecs);
  virtual bool supportsProtocol(const scString &protocol);
  virtual bool getOwnAddress(const scString &protocol, scMessageAddress &output);
protected:
  void splitTopic(const scString &rawHost, scString &host, scString &topic);
  scString extractTopic(const scString &addr);
protected:
  scString m_protocol; 
  scString m_zmqProtocol; 
  scString m_address; 
  uint m_inactTimeout; // inactivity timeout for connections
  std::auto_ptr<scEnvelopeSerializerBase> m_serializer; 
  zmContext *m_context;
};

class zmConnectionOut: public scConnection {
public:
  // construction
  zmConnectionOut(zmContext *context);
  virtual ~zmConnectionOut();
  // exec
  bool connect(const scString &address, bool usePublish);  
  virtual void close();
  virtual bool isConnected();  
  void send(char *ptr, size_t asize);
protected:  
  void checkConnected();
protected:
  zmSocketGuard m_socket;
  zmContext *m_context;
};

class zmGateInput: public zmGate {
public:
  zmGateInput(zmContext *context);
  virtual ~zmGateInput();
  virtual void init();
  virtual int run();
protected:    
  bool pull();
  void putEnvelopeStr(const scString &str);
protected:    
  std::auto_ptr<zmq::socket_t> m_socket;
  bool m_connected;
  scString m_topic;
};

class zmGateOutput: public zmGate {
public:
  zmGateOutput(zmContext *context);
  virtual ~zmGateOutput();
  virtual int run();
protected:
  void transmitEnvelope(scEnvelope *envelope);
  zmConnectionOut *findConnection(const scString &connectionId);
  zmConnectionOut *prepareConnection(const scMessageAddress &address);
protected:
  scConnectionPool m_connections;    
};

//----------------------------------------------------------------------------------
// Local classes - bodies
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// zmContextBase
//----------------------------------------------------------------------------------
zmContextBase *zmContextBase::newContext()
{
  return new zmContext();
}

//----------------------------------------------------------------------------------
// zmContext
//----------------------------------------------------------------------------------
zmContext::zmContext()
{
  Log::addTrace("Init zmq...");
  //m_context.reset(new zmq::context_t(1, 1));
  m_context.reset(new zmq::context_t(1));
  Log::addTrace("Init zmq done!");
}

zmContext::~zmContext()
{
  m_context.reset(); //DEBUG
}

void zmContext::clear()
{
  m_context.reset(); 
}

zmq::context_t &zmContext::getHandle()
{
  return *m_context;
}

//----------------------------------------------------------------------------------
// zmGate
//----------------------------------------------------------------------------------
zmGate::zmGate(zmContext *context): scMessageGate()
{
  m_context = context;
  m_serializer.reset(new scEnvSerializerJsonYajl());
  m_inactTimeout = SC_ZMQ_DEF_INACT_CONN_TIMEOUT;
}

zmGate::~zmGate()
{
}

void zmGate::setZmqProtocol(const scString &protocol)
{
  m_zmqProtocol = protocol;
}

void zmGate::setProtocol(const scString &protocol)
{
  m_protocol = protocol;
}

void zmGate::setAddress(const scString &address)
{
  m_address = address;
}

void zmGate::setInactTimeout(uint msecs)
{
  m_inactTimeout = msecs;
}

bool zmGate::supportsProtocol(const scString &protocol)
{
  return (m_protocol == protocol);
}

void zmGate::splitTopic(const scString &rawHost, scString &host, scString &topic)
{
  scString::size_type pos = rawHost.find("+");
  host = rawHost;
  topic = "";
  if (pos != scString::npos) {
    host = rawHost.substr(0, pos);
    topic = rawHost.substr(pos + 1);
  }
}

scString zmGate::extractTopic(const scString &addr)
{
  scString host, topic;
  splitTopic(addr, host, topic);
  return topic;
}

bool zmGate::getOwnAddress(const scString &protocol, scMessageAddress &output)
{
  bool res = false;
  if (protocol == m_protocol) 
  {
    output.clear();
    output.setProtocol(protocol);
    output.setHost(m_address);    
    res = true;
  }
  return res;
}

//----------------------------------------------------------------------------------
// zmGateInput
//----------------------------------------------------------------------------------
zmGateInput::zmGateInput(zmContext *context): zmGate(context)
{
}

zmGateInput::~zmGateInput()
{
}

void zmGateInput::init()
{
  scString host, topic;
  splitTopic(m_address, host, topic); 
  bool useSub = (!topic.empty());

  if (useSub)
    m_socket.reset(new zmq::socket_t(m_context->getHandle(), ZMQ_SUB));
  else
    m_socket.reset(new zmq::socket_t(m_context->getHandle(), ZMQ_REP));
    
  try {
    m_socket->bind(host.c_str());
    if (!topic.empty()) {
      scString rawTopic(topic + SC_ZMQ_TOPIC_SEP);
      m_socket->setsockopt(ZMQ_SUBSCRIBE, rawTopic.c_str(), rawTopic.length());
      m_topic = topic;
    }  
  } catch(...) {
    m_socket.reset();
    throw;
  }  
}

int zmGateInput::run()
{ 
  int res = 0;
  if (m_connected)
  {
    while(pull())
    {  
      res++;
    }  
  }
  return res;
}

bool zmGateInput::pull()
{
  bool res = false;
  
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-execute-zmq");
#endif     
  
  zmq::message_t msg;  
  int rc = m_socket->recv(&msg, ZMQ_NOBLOCK); 
  
#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-execute-zmq");
  Timer::stop("msg-total");
#endif     
  
  if (rc) {
    scString envelopeStr((char*) msg.data());
    if (!m_topic.empty())
    {
      if (!envelopeStr.empty())
        if (envelopeStr.substr(0, m_topic.length())+SC_ZMQ_TOPIC_SEP == m_topic+SC_ZMQ_TOPIC_SEP)
          envelopeStr = envelopeStr.substr(m_topic.length()+SC_ZMQ_TOPIC_SEP.length());
    }  
    Counter::inc("msg-size", envelopeStr.length());
    putEnvelopeStr(envelopeStr);
    res = true;
  }
  return res;
}

void zmGateInput::putEnvelopeStr(const scString &str)
{
  std::auto_ptr<scEnvelope> guard(new scEnvelope());
  m_serializer->convFromString(str, *guard);
  handleMsgReceived(*guard);
  put(guard.release());
}

//----------------------------------------------------------------------------------
// zmGateOutput
//----------------------------------------------------------------------------------
zmGateOutput::zmGateOutput(zmContext *context): zmGate(context)
{
}

zmGateOutput::~zmGateOutput()
{
}

int zmGateOutput::run()
{
  int res = 0;
  std::auto_ptr<scEnvelope> envelopeGuard;
    
  m_connections.checkActive();  

  while(!empty()) 
  {
    envelopeGuard.reset(get());
    res++;
    try {
      transmitEnvelope(envelopeGuard.get());
    }
    catch (scError &e) {
      e.addDetails("out-addr", scDataNode(envelopeGuard->getReceiver().getAsString())); 
      handleTransmitError(*envelopeGuard.get(), e);
    }      
    catch(const std::exception& e) {
      scString msg = scString("0MQ-Transmit - exception (std): ") + e.what();
      scString dets = scString("out-addr: ") + envelopeGuard->getReceiver().getAsString();
      handleTransmitError(*envelopeGuard.get(), SC_MSG_STATUS_EXCEPTION, msg, dets);
    }  
    catch(...) {
      scString msg = scString("0MQ-Transmit - exception (unknown)");
      scString dets = scString("out-addr: ") + envelopeGuard->getReceiver().getAsString();
      handleTransmitError(*envelopeGuard.get(), SC_MSG_STATUS_EXCEPTION, msg, dets);
    }  
  } // while     

  return res;
}

void zmGateOutput::transmitEnvelope(scEnvelope *envelope)
{
  scString dataStr;  

  m_serializer->convToString(*envelope, dataStr); 
  
  zmConnectionOut *item = prepareConnection(envelope->getReceiver());
  if (item == SC_NULL)
    throw scError("zmq gate.execute failed - connection failed");
      
  if (item) {    
     scString topic = extractTopic(envelope->getReceiver().getHost());
     if (!topic.empty())
       dataStr = topic + SC_ZMQ_TOPIC_SEP + dataStr;

     if (dataStr.length() >= SC_ZMQ_MAX_MSG_SIZE)
       throw scError("ZMQ message too long ("+toString(dataStr.length())+")");
       
     Counter::inc("msg-size", dataStr.length());
       
     scChar data[SC_ZMQ_MAX_MSG_SIZE];
     //wxStrncpy(data, dataStr.c_str(), dataStr.length()+1);
     strncpy(data, dataStr.c_str(), dataStr.length()+1);
     
  Counter::inc("msg-total");
     
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-execute-zmq");
#endif     

  handleMsgReadyForSend(*envelope);
  item->send(data, (dataStr.length()+1)*sizeof(scChar));
  handleMsgSent(*envelope);

#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-execute-zmq");
  Timer::stop("msg-total");
#endif     
  }
  else
    throw scError("ZMQ gate.connect failed");  
}

zmConnectionOut *zmGateOutput::prepareConnection(const scMessageAddress &address)
{
  scString rawHost = address.getHost();
  scString topic, host;
  
  splitTopic(rawHost, host, topic);
  
  scString connectionId = address.getProtocol() + host;
  scString connectionStr;
  
  if (m_address.empty())
    connectionStr = m_zmqProtocol + "://" + host;
  else  
    connectionStr = m_address;
  
  zmConnectionOut *item = findConnection(connectionId);
  if (item == SC_NULL)
  {
    std::auto_ptr<zmConnectionOut> connGuard;
    connGuard.reset(new zmConnectionOut(this->m_context));
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-connect-zmq");
#endif    
    connGuard.get()->connect(connectionStr, !topic.empty());
#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-connect-zmq");
  Timer::stop("msg-total");
#endif    
    item = connGuard.get();
    item->setInactTimeout(this->m_inactTimeout);
    m_connections.add(connectionId, connGuard.release());    
  }
  return item;
}

zmConnectionOut *zmGateOutput::findConnection(const scString &connectionId)
{
  zmConnectionOut *res = dynamic_cast<zmConnectionOut *>(m_connections.find(connectionId));
  return res;
}

//----------------------------------------------------------------------------------
// zmConnectionOut
//----------------------------------------------------------------------------------
zmConnectionOut::zmConnectionOut(zmContext *context): scConnection(), m_context(context)
{
}

zmConnectionOut::~zmConnectionOut()
{
}

bool zmConnectionOut::isConnected()
{
  return (m_socket.get() != SC_NULL);
}

bool zmConnectionOut::connect(const scString &address, bool usePublish)
{
  bool res = isConnected();
  if (!res) {
    if (usePublish)
      m_socket.reset(new zmq::socket_t(m_context->getHandle(), ZMQ_PUB));
    else  
      m_socket.reset(new zmq::socket_t(m_context->getHandle(), ZMQ_REQ));
    try {
      m_socket->connect(address.c_str());
    } 
    catch(...) {
      m_socket.reset();
      throw;
    }  
    res = true;
  }    
  signalConnected();
  return res;
}

void zmConnectionOut::close()
{
  if (isConnected())
  {
    m_socket.reset();
  }
}

void zmConnectionOut::checkConnected()
{
  if (!isConnected())
    throw scError("ZMQ connection not active!");
}

void zmConnectionOut::send(char *ptr, size_t asize)
{
  checkConnected();
  zmq::message_t msg (asize);      
  memcpy(msg.data(), ptr, msg.size()); 
  m_socket->send(msg);
  signalUsed();
}

//----------------------------------------------------------------------------------
// zmGateFactory
//----------------------------------------------------------------------------------
zmGateFactory::zmGateFactory(zmContextBase *ctx): scGateFactory(), m_context(ctx)
{
}

zmGateFactory::~zmGateFactory()
{
}

scMessageGate *zmGateFactory::createGate(bool input, const scDataNode &params, const scString &protocol) const
{
  std::auto_ptr<zmGate> res;
  if (input) {
    res.reset(new zmGateInput(static_cast<zmContext *>(m_context)));
    if (params.size()>0)  
      static_cast<zmGateInput *>(res.get())->setAddress(params.getString(0));  
  }     
  else { 
    res.reset(new zmGateOutput(static_cast<zmContext *>(m_context)));
    if (params.size()>0)  
      static_cast<zmGateOutput *>(res.get())->setAddress(params.getString(0));  

    if (params.size()>1) { 
      uint timeout = params.getUInt(1);
      static_cast<zmGateOutput *>(res.get())->setInactTimeout(timeout);  
    }  
  }  
  return res.release();
}

scMessageGate *zmGateFactoryForTcp::createGate(bool input, const scDataNode &params, const scString &protocol) const
{
  std::auto_ptr<zmGate> res(dynamic_cast<zmGate *>(zmGateFactory::createGate(input, params, "")));
  res->setProtocol("tcp");  
  res->setZmqProtocol("tcp");  
  return res.release();
}

scMessageGate *zmGateFactoryForPgm::createGate(bool input, const scDataNode &params, const scString &protocol) const
{
  std::auto_ptr<zmGate> res(dynamic_cast<zmGate *>(zmGateFactory::createGate(input, params, "")));
  res->setProtocol("pgm");  
  res->setZmqProtocol("pgm");  
  return res.release();
}

scMessageGate *zmGateFactoryForIpc::createGate(bool input, const scDataNode &params, const scString &protocol) const
{
  std::auto_ptr<zmGate> res(dynamic_cast<zmGate *>(zmGateFactory::createGate(input, params, "")));
  res->setProtocol("ipc");  
  res->setZmqProtocol("tcp");  
  return res.release();
}

