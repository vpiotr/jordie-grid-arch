/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesGate.cpp
// Project:     grdLib
// Purpose:     Win32 named pipes gate.
// Author:      Piotr Likus
// Modified by:
// Created:     29/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/W32NamedPipesGate.h"
#include "grd/W32NamedPipesGatesRaw.h"
#include "grd/W32NamedPipesInputThread.h"
#include "grd/W32NamedPipesQueue.h"

#include "grd/EnvSerializerJsonYajl.h"
#include "grd/MessageGate.h"
#include "grd/MessageAddress.h"
#include "grd/Connection.h"
#include "grd/ConnectionPool.h"
#include "grd/MessageConst.h"

#include "perf/Timer.h"
#include "perf/Counter.h"
#include "perf/Log.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

//using namespace boost::interprocess;
using namespace perf;
using namespace std;

//----------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------
const uint GRD_W32_NP_DEF_INACT_CONN_TIMEOUT = 30000;
const uint GRD_W32_NP_MAX_MSG_SIZE = 65536;
const uint GRD_W32_NP_MAX_MSG_COUNT = 512;
const uint GRD_W32_NP_DEF_PIPE_LIMIT = 5; // not used now

//----------------------------------------------------------------------------------
// Local classes - declarations
//----------------------------------------------------------------------------------
class grdW32NamedPipesGate: public scMessageGate {
public:
  grdW32NamedPipesGate();
  virtual ~grdW32NamedPipesGate();
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

class grdW32NamedPipesLog: public grdW32NamedPipesLogIntf {
public:
  virtual void writeError(const std::string &errorMsg, unsigned int errorCode) {
    Log::addError(errorCode, errorMsg);
  }

  virtual void writeWarning(const std::string &message) {
    Log::addWarning(message);
  }

  virtual void writeDebug(const std::string &message) {
    Log::addDebug(message);
  }
};

class grdW32NamedPipesConnectionOut: public scConnection {
public:
  // construction
  grdW32NamedPipesConnectionOut();
  virtual ~grdW32NamedPipesConnectionOut();
  // exec
  bool connect(const scString &address);  
  virtual void close();
  virtual bool isConnected();  
  void send(char *ptr, size_t asize);
  int run();
protected:  
  void checkConnected();
protected:
  std::auto_ptr<grdW32NamedPipesGateOutRaw> m_handle;
  grdW32NamedPipesLog m_log;
  grdW32NamedPipesStringMessage m_stringMsg;
  grdW32NamedPipesQueue m_queue;
};

class grdW32NamedPipesGateInput: public grdW32NamedPipesGate {
public:
  grdW32NamedPipesGateInput();
  virtual ~grdW32NamedPipesGateInput();
  virtual void init();
  virtual int run();
protected:    
  bool pull(char *buffer, size_t buffer_size);
  void putEnvelopeStr(const scString &str);
  bool isConnected();
  void tryOpen();
  void close();
protected:    
  std::auto_ptr<grdW32NamedPipesGateInRaw> m_handle;
  std::auto_ptr<grdW32NamedPipesInputThread> m_inputThread;
  grdW32NamedPipesLog m_log;
  grdW32NamedPipesStringMessage m_stringMsg;
  grdW32NamedPipesQueue m_queue;
};

class grdW32NamedPipesGateOutput: public grdW32NamedPipesGate {
public:
  grdW32NamedPipesGateOutput();
  virtual ~grdW32NamedPipesGateOutput();
  virtual int run();
protected:
  void transmitEnvelope(scEnvelope *envelope);
  grdW32NamedPipesConnectionOut *findConnection(const scString &connectionId);
  grdW32NamedPipesConnectionOut *prepareConnection(const scMessageAddress &address);
  int runConnections(); 
protected:
  scConnectionPool m_connections;    
};

//----------------------------------------------------------------------------------
// Implementation part
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// grdW32NamedPipesGate
//----------------------------------------------------------------------------------
grdW32NamedPipesGate::grdW32NamedPipesGate(): scMessageGate()
{
  m_serializer.reset(new scEnvSerializerJsonYajl());
  m_inactTimeout = GRD_W32_NP_DEF_INACT_CONN_TIMEOUT;
}

grdW32NamedPipesGate::~grdW32NamedPipesGate()
{
}

void grdW32NamedPipesGate::setProtocol(const scString &protocol)
{
  m_protocol = protocol;
}

void grdW32NamedPipesGate::setAddress(const scString &address)
{
  m_address = address;
}

void grdW32NamedPipesGate::setInactTimeout(uint msecs)
{
  m_inactTimeout = msecs;
}

bool grdW32NamedPipesGate::supportsProtocol(const scString &protocol)
{
  return (m_protocol == protocol);
}

size_t grdW32NamedPipesGate::getBufferSize()
{
  return GRD_W32_NP_MAX_MSG_SIZE+1;
}

void grdW32NamedPipesGate::initBuffer()
{
  m_buffer.reset(new char[getBufferSize()]); 
}

bool grdW32NamedPipesGate::getOwnAddress(const scString &protocol, scMessageAddress &output)
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
// grdW32NamedPipesGateInput
//----------------------------------------------------------------------------------
grdW32NamedPipesGateInput::grdW32NamedPipesGateInput(): grdW32NamedPipesGate()
{
  initBuffer();
}

grdW32NamedPipesGateInput::~grdW32NamedPipesGateInput()
{
  close();
}

void grdW32NamedPipesGateInput::close()
{
  if (m_inputThread.get() != NULL)
    m_inputThread->terminate();
  m_handle.reset();
}

void grdW32NamedPipesGateInput::init()
{
  tryOpen();
}

void grdW32NamedPipesGateInput::tryOpen()
{
  try {
    close();
    m_handle.reset(new grdW32NamedPipesGateInRaw());

    m_handle->setPipeLimit(GRD_W32_NP_DEF_PIPE_LIMIT);
    m_handle->setPipePath(m_address.c_str());

    m_inputThread.reset(new grdW32NamedPipesInputThread(m_handle.get()));

    m_handle->setLog(&m_log);
    m_handle->setMessagePrototype(&m_stringMsg);
    m_handle->setMessageQueue(&m_queue);

    m_inputThread->setLog(&m_log);
    m_inputThread->run();

  } catch(...) {
    m_handle.reset();
    throw;
  }  
}

bool grdW32NamedPipesGateInput::isConnected()
{
  return (m_handle.get() != SC_NULL);
}

int grdW32NamedPipesGateInput::run()
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

bool grdW32NamedPipesGateInput::pull(char *buffer, size_t buffer_size)
{
  bool res = false;
  size_t recvd_size;
  uint priority;
  
  if (isConnected())
  {
    scString msg;

    try {
      res = m_handle->hasMessage();
      if (res) {
        //m_handle->getMessage(msg);
        m_handle->getMessage(m_stringMsg);
        m_stringMsg.getValue(msg);
      }
    }
    catch(...) {
      res = false;
      // auto-close on exception
      try { close(); } catch(...) {}
      throw;
    }
    
    if (res) {
      Counter::inc("msg-size", msg.length());
      putEnvelopeStr(msg);
    }
  }
  return res;
}

void grdW32NamedPipesGateInput::putEnvelopeStr(const scString &str)
{
  std::auto_ptr<scEnvelope> guard(new scEnvelope());
  m_serializer->convFromString(str, *guard);
  handleMsgReceived(*guard);
  put(guard.release());
}

//----------------------------------------------------------------------------------
// grdW32NamedPipesGateOutput
//----------------------------------------------------------------------------------
grdW32NamedPipesGateOutput::grdW32NamedPipesGateOutput(): grdW32NamedPipesGate()
{
  initBuffer();
}

grdW32NamedPipesGateOutput::~grdW32NamedPipesGateOutput()
{
}

int grdW32NamedPipesGateOutput::run()
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
      scString msg = scString("W32-NP-Transmit - exception (std): ") + e.what();
      scString dets = scString("out-addr: ") + envelopeGuard->getReceiver().getAsString();
      handleTransmitError(*envelopeGuard.get(), SC_MSG_STATUS_EXCEPTION, msg, dets);
    }  
    catch(...) {
      scString msg = scString("W32-NP-Transmit - exception (unknown)");
      scString dets = scString("out-addr: ") + envelopeGuard->getReceiver().getAsString();
      handleTransmitError(*envelopeGuard.get(), SC_MSG_STATUS_EXCEPTION, msg, dets);
    }  
  } // while     

  res += runConnections();

  return res;
}

void grdW32NamedPipesGateOutput::transmitEnvelope(scEnvelope *envelope)
{
  scString dataStr;  

  m_serializer->convToString(*envelope, dataStr); 
  
  grdW32NamedPipesConnectionOut *item = prepareConnection(envelope->getReceiver());
  if (item == SC_NULL)
    throw scError("W32 named pipes gate.execute failed - connection failed");
      
  if (item) {    
     if (dataStr.length() >= getBufferSize())
       throw scError("W32 named pipes message too long ("+toString(dataStr.length())+")");

     //wxStrncpy(m_buffer.get(), dataStr.c_str(), dataStr.length()+1);
     strncpy(m_buffer.get(), dataStr.c_str(), dataStr.length()+1);

  Counter::inc("msg-total");
  Counter::inc("msg-size", (dataStr.length() + 1) * sizeof(scChar));
     
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-execute-wnp");
#endif     

  handleMsgReadyForSend(*envelope);
  item->send(m_buffer.get(), (dataStr.length()+1)*sizeof(scChar));
  handleMsgSent(*envelope);

#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-execute-wnp");
  Timer::stop("msg-total");
#endif     
  }
  else
    throw scError("W32 named pipes gate.connect failed");  
}

grdW32NamedPipesConnectionOut *grdW32NamedPipesGateOutput::prepareConnection(const scMessageAddress &address)
{
  scString host = address.getHost();
  
  scString connectionId = host;
  scString connectionStr;
  
  if (m_address.empty())
    connectionStr = host;
  else  
    connectionStr = m_address;
  
  grdW32NamedPipesConnectionOut *item = findConnection(connectionId);
  if (item == SC_NULL)
  {
    std::auto_ptr<grdW32NamedPipesConnectionOut> connGuard;
    connGuard.reset(new grdW32NamedPipesConnectionOut());
    
#ifdef SC_TIMER_ENABLED
  Timer::start("msg-total");
  Timer::start("msg-connect-wnp");
#endif    
    connGuard.get()->connect(connectionStr);
#ifdef SC_TIMER_ENABLED
  Timer::stop("msg-connect-wnp");
  Timer::stop("msg-total");
#endif    
    item = connGuard.get();
    item->setInactTimeout(this->m_inactTimeout);
    m_connections.add(connectionId, connGuard.release());    
  }
  return item;
}

grdW32NamedPipesConnectionOut *grdW32NamedPipesGateOutput::findConnection(const scString &connectionId)
{
  grdW32NamedPipesConnectionOut *res = dynamic_cast<grdW32NamedPipesConnectionOut *>(m_connections.find(connectionId));
  return res;
}

struct grdW32NamedPipesOutRunner {
  grdW32NamedPipesOutRunner(int &cnt): m_cntRef(cnt) { cnt = 0; }
  void operator()(scConnection *out) { 
    m_cntRef += static_cast<grdW32NamedPipesConnectionOut *>(out)->run();
  }
private:
  int &m_cntRef;
};

int grdW32NamedPipesGateOutput::runConnections()
{
  int cnt;
  m_connections.forEach(grdW32NamedPipesOutRunner(cnt));
  return cnt;
}

//----------------------------------------------------------------------------------
// grdW32NamedPipesConnectionOut
//----------------------------------------------------------------------------------
grdW32NamedPipesConnectionOut::grdW32NamedPipesConnectionOut(): scConnection()
{
}

grdW32NamedPipesConnectionOut::~grdW32NamedPipesConnectionOut()
{
  performAutoClose();
}

bool grdW32NamedPipesConnectionOut::isConnected()
{
  return (m_handle.get() != SC_NULL);
}

bool grdW32NamedPipesConnectionOut::connect(const scString &address)
{
  bool res = isConnected();  
  if (!res) {
    try {
      m_handle.reset(new grdW32NamedPipesGateOutRaw());
      m_handle->setPipePath(address.c_str());      
 
      m_handle->setLog(&m_log);
      m_handle->setMessagePrototype(&m_stringMsg);
      m_handle->setMessageQueue(&m_queue);
    } catch(...) {
      m_handle.reset();
      throw;
    }  
    res = true;
  }    
  signalConnected();
  return res;
}

void grdW32NamedPipesConnectionOut::close()
{
  if (isConnected())
  {
    m_handle.reset();
  }
}

void grdW32NamedPipesConnectionOut::checkConnected()
{
  if (!isConnected())
    throw scError("BMQ connection not active!");
}

void grdW32NamedPipesConnectionOut::send(char *ptr, size_t asize)
{
  checkConnected();

  //m_handle->putMessage(scString(ptr, asize));
  m_stringMsg.setValue(string(ptr, asize));
  m_handle->putMessage(m_stringMsg);
  m_handle->run();

  signalUsed();
}

int grdW32NamedPipesConnectionOut::run()
{
  return m_handle->run();
}

//----------------------------------------------------------------------------------
// grdW32NamedPipesGateFactory
//----------------------------------------------------------------------------------
grdW32NamedPipesGateFactory::grdW32NamedPipesGateFactory(): scGateFactory() 
{
}  
grdW32NamedPipesGateFactory::~grdW32NamedPipesGateFactory()
{
}
scMessageGate *grdW32NamedPipesGateFactory::createGate(bool input, const scDataNode &params, const scString &protocol) const
{
  std::auto_ptr<grdW32NamedPipesGate> res;
  if (input) {
    res.reset(new grdW32NamedPipesGateInput());
    if (params.size()>0)  
      static_cast<grdW32NamedPipesGateInput *>(res.get())->setAddress(params.getString(0));  
  }    
  else { 
    res.reset(new grdW32NamedPipesGateOutput());
    if (params.size()>0)  
      static_cast<grdW32NamedPipesGateOutput *>(res.get())->setAddress(params.getString(0));  

    if (params.size()>1) { 
      uint timeout = params.getUInt(1);
      static_cast<grdW32NamedPipesGateOutput *>(res.get())->setInactTimeout(timeout);  
    }  
  }  
  res->setProtocol(protocol);
  return res.release();

}
