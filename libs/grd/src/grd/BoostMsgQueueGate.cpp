/////////////////////////////////////////////////////////////////////////////
// Name:        BoostMsgQueue.cpp
// Project:     grdLib
// Purpose:     Boost-based message gate.
// Author:      Piotr Likus
// Modified by:
// Created:     17/04/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/ipc/message_queue.hpp>

#include "grd/BoostMsgQueueGate.h"
#include "grd/EnvSerializerJsonYajl.h"
#include "grd/MessageGate.h"
#include "grd/MessageAddress.h"
#include "grd/Connection.h"
#include "grd/ConnectionPool.h"
#include "grd/MessageConst.h"

#include "perf/Timer.h"
#include "perf/Counter.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace boost::interprocess;
using namespace perf;

//----------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------
const uint SC_BMQ_DEF_INACT_CONN_TIMEOUT = 30000;
const uint SC_BMQ_MAX_MSG_SIZE = 65536;
const uint SC_BMQ_MAX_MSG_COUNT = 512;

//----------------------------------------------------------------------------------
// Local classes - declarations
//----------------------------------------------------------------------------------
class grdBmqGate: public scMessageGate {
public:
  grdBmqGate();
  virtual ~grdBmqGate();
  void setProtocol(const scString &protocol);
  void setAddress(const scString &address);
  void setInactTimeout(uint msecs);
  virtual bool supportsProtocol(const scString &protocol);
  virtual bool getOwnAddress(const scString &protocol, scMessageAddress &output);
protected:  
  size_t getBufferSize();
  void initBuffer();
protected:
  scString m_protocol; 
  scString m_address;
  uint m_inactTimeout; // inactivity timeout for connections
  boost::shared_ptr<scEnvelopeSerializerBase> m_serializer; 
  boost::shared_ptr<char> m_buffer; 
};

class grdBmqConnectionOut: public scConnection {
public:
  // construction
  grdBmqConnectionOut();
  virtual ~grdBmqConnectionOut();
  // exec
  bool connect(const scString &address);  
  virtual void close();
  virtual bool isConnected();  
  void send(char *ptr, size_t asize);
protected:  
  void checkConnected();
protected:
  std::auto_ptr<message_queue> m_handle;
};

class grdBmqGateInput: public grdBmqGate {
public:
  grdBmqGateInput();
  virtual ~grdBmqGateInput();
  virtual void init();
  virtual int run();
protected:    
  bool pull(char *buffer, size_t buffer_size);
  void putEnvelopeStr(const scString &str);
  bool isConnected();
  void tryOpen();
  void close();
protected:    
  std::auto_ptr<message_queue> m_handle;
};

class grdBmqGateOutput: public grdBmqGate {
public:
  grdBmqGateOutput();
  virtual ~grdBmqGateOutput();
  virtual int run();
protected:
  void transmitEnvelope(scEnvelope *envelope);
  grdBmqConnectionOut *findConnection(const scString &connectionId);
  grdBmqConnectionOut *prepareConnection(const scMessageAddress &address);
protected:
  scConnectionPool m_connections;    
};

//----------------------------------------------------------------------------------
// Implementation part
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// grdBmqGate
//----------------------------------------------------------------------------------
grdBmqGate::grdBmqGate(): scMessageGate()
{
  m_serializer.reset(new scEnvSerializerJsonYajl());
  m_inactTimeout = SC_BMQ_DEF_INACT_CONN_TIMEOUT;
}

grdBmqGate::~grdBmqGate()
{
}

void grdBmqGate::setProtocol(const scString &protocol)
{
  m_protocol = protocol;
}

void grdBmqGate::setAddress(const scString &address)
{
  m_address = address;
}

void grdBmqGate::setInactTimeout(uint msecs)
{
  m_inactTimeout = msecs;
}

bool grdBmqGate::supportsProtocol(const scString &protocol)
{
  return (m_protocol == protocol);
}

size_t grdBmqGate::getBufferSize()
{
  return SC_BMQ_MAX_MSG_SIZE+1;
}

void grdBmqGate::initBuffer()
{
  m_buffer.reset(new char[getBufferSize()]); 
}

bool grdBmqGate::getOwnAddress(const scString &protocol, scMessageAddress &output)
{
  bool res = false;
  if (protocol == m_protocol) 
  {
    output.clear();
    output.setProtocol(protocol);
    output.setHost(m_address);    
    output.setNode(getOwnerName());    
    res = true;
  }
  return res;
}

//----------------------------------------------------------------------------------
// grdBmqGateInput
//----------------------------------------------------------------------------------
grdBmqGateInput::grdBmqGateInput(): grdBmqGate()
{
  initBuffer();
}

grdBmqGateInput::~grdBmqGateInput()
{
  close();
}

void grdBmqGateInput::close()
{
  m_handle.reset();
  if (!m_address.empty()) {
    message_queue::remove(m_address.c_str());  
  }
}

void grdBmqGateInput::init()
{
  tryOpen();
}

void grdBmqGateInput::tryOpen()
{
  try {
    close();
    m_handle.reset(new message_queue(create_only, m_address.c_str(), SC_BMQ_MAX_MSG_COUNT, SC_BMQ_MAX_MSG_SIZE));
  } catch(...) {
    m_handle.reset();
    throw;
  }  
}

bool grdBmqGateInput::isConnected()
{
  return (m_handle.get() != SC_NULL);
}

int grdBmqGateInput::run()
{ 
  int res = 0;
  if (!isConnected())
    tryOpen();

  if (isConnected())
  {
    while(pull(m_buffer.get(), getBufferSize()))
    {  
      res++;
    }  
  }
  return res;
}

bool grdBmqGateInput::pull(char *buffer, size_t buffer_size)
{
  bool res = false;
  size_t recvd_size;
  uint priority;
  
  if (isConnected())
  {
    try {
      res = m_handle->try_receive(buffer, buffer_size, recvd_size, priority);  
    }
    catch(...) {
      res = false;
      // auto-close on exception
      try { close(); } catch(...) {}
      throw;
    }
    
    if (res) {
      buffer[recvd_size+1] = '\0';
      scString envelopeStr(buffer);
      Counter::inc("msg-size", envelopeStr.length());
      putEnvelopeStr(envelopeStr);
    }
  }
  return res;
}

void grdBmqGateInput::putEnvelopeStr(const scString &str)
{
  std::auto_ptr<scEnvelope> guard(new scEnvelope());
  m_serializer->convFromString(str, *guard);
  handleMsgReceived(*guard);
  put(guard.release());
}

//----------------------------------------------------------------------------------
// grdBmqGateOutput
//----------------------------------------------------------------------------------
grdBmqGateOutput::grdBmqGateOutput(): grdBmqGate()
{
  initBuffer();
}

grdBmqGateOutput::~grdBmqGateOutput()
{
}

int grdBmqGateOutput::run()
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
      e.addDetails(scDataNode(envelopeGuard->getReceiver().getAsString())); 
      handleTransmitError(*envelopeGuard.get(), e);
    }      
    catch(const std::exception& e) {
      scString msg = scString("BMQ-Transmit - exception (std): ") + e.what();
      scString dets = scString("out-addr: ") + envelopeGuard->getReceiver().getAsString();
      handleTransmitError(*envelopeGuard.get(), SC_MSG_STATUS_EXCEPTION, msg, dets);
    }  
    catch(...) {
      scString msg = scString("BMQ-Transmit - exception (unknown)");
      scString dets = scString("out-addr: ") + envelopeGuard->getReceiver().getAsString();
      handleTransmitError(*envelopeGuard.get(), SC_MSG_STATUS_EXCEPTION, msg, dets);
    }  
  } // while     

  return res;
}

void grdBmqGateOutput::transmitEnvelope(scEnvelope *envelope)
{
  scString dataStr;  

  m_serializer->convToString(*envelope, dataStr); 
  
  grdBmqConnectionOut *item = prepareConnection(envelope->getReceiver());
  if (item == SC_NULL)
    throw scError("bmq gate.execute failed - connection failed");
      
  if (item) {    
     if (dataStr.length() >= getBufferSize())
       throw scError("BMQ message too long ("+toString(dataStr.length())+")");

     //wxStrncpy(m_buffer.get(), dataStr.c_str(), dataStr.length()+1);
     strncpy(m_buffer.get(), dataStr.c_str(), dataStr.length()+1);

  Counter::inc("msg-total");
  Counter::inc("msg-size", (dataStr.length() + 1) * sizeof(scChar));
     
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-execute-bmq");
#endif     

  handleMsgReadyForSend(*envelope);
  item->send(m_buffer.get(), (dataStr.length()+1)*sizeof(scChar));
  handleMsgSent(*envelope);

#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-execute-bmq");
  Timer::stop("msg-total");
#endif     
  }
  else
    throw scError("BMQ gate.connect failed");  
}

grdBmqConnectionOut *grdBmqGateOutput::prepareConnection(const scMessageAddress &address)
{
  scString host = address.getHost();
  
  scString connectionId = host;
  scString connectionStr;
  
  if (m_address.empty())
    connectionStr = host;
  else  
    connectionStr = m_address;
  
  grdBmqConnectionOut *item = findConnection(connectionId);
  if (item == SC_NULL)
  {
    std::auto_ptr<grdBmqConnectionOut> connGuard;
    connGuard.reset(new grdBmqConnectionOut());
    
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-connect-bmq");
#endif    
    connGuard.get()->connect(connectionStr);
#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-connect-bmq");
  Timer::stop("msg-total");
#endif    
    item = connGuard.get();
    item->setInactTimeout(this->m_inactTimeout);
    m_connections.add(connectionId, connGuard.release());    
  }
  return item;
}

grdBmqConnectionOut *grdBmqGateOutput::findConnection(const scString &connectionId)
{
  grdBmqConnectionOut *res = dynamic_cast<grdBmqConnectionOut *>(m_connections.find(connectionId));
  return res;
}

//----------------------------------------------------------------------------------
// grdBmqConnectionOut
//----------------------------------------------------------------------------------
grdBmqConnectionOut::grdBmqConnectionOut(): scConnection()
{
}

grdBmqConnectionOut::~grdBmqConnectionOut()
{
  performAutoClose();
}

bool grdBmqConnectionOut::isConnected()
{
  return (m_handle.get() != SC_NULL);
}

bool grdBmqConnectionOut::connect(const scString &address)
{
  bool res = isConnected();  
  if (!res) {
    try {
      m_handle.reset(new message_queue(open_only, address.c_str()));
    } catch(...) {
      m_handle.reset();
      throw;
    }  
    res = true;
  }    
  signalConnected();
  return res;
}

void grdBmqConnectionOut::close()
{
  if (isConnected())
  {
    m_handle.reset();
  }
}

void grdBmqConnectionOut::checkConnected()
{
  if (!isConnected())
    throw scError("BMQ connection not active!");
}

void grdBmqConnectionOut::send(char *ptr, size_t asize)
{
  checkConnected();
  m_handle->send(ptr, asize, 0);
  signalUsed();
}

//----------------------------------------------------------------------------------
// grdBmqGateFactory
//----------------------------------------------------------------------------------
grdBmqGateFactory::grdBmqGateFactory(): scGateFactory() 
{
}  
grdBmqGateFactory::~grdBmqGateFactory()
{
}
scMessageGate *grdBmqGateFactory::createGate(bool input, const scDataNode &params, const scString &protocol) const
{
  std::auto_ptr<grdBmqGate> res;
  if (input) {
    res.reset(new grdBmqGateInput());
    if (params.size()>0)  
      static_cast<grdBmqGateInput *>(res.get())->setAddress(params.getString(0));  
  }    
  else { 
    res.reset(new grdBmqGateOutput());
    if (params.size()>0)  
      static_cast<grdBmqGateOutput *>(res.get())->setAddress(params.getString(0));  

    if (params.size()>1) { 
      uint timeout = params.getUInt(1);
      static_cast<grdBmqGateOutput *>(res.get())->setInactTimeout(timeout);  
    }  
  }  
  res->setProtocol(protocol);
  return res.release();

}
