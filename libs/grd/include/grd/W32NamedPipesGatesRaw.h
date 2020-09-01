/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesGatesRaw.h
// Project:     grdLib
// Purpose:     Gates for named pipes - raw (w/o framework) version.
// Author:      Piotr Likus
// Modified by:
// Created:     28/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _W32NAMPIPGTRAW_H__
#define _W32NAMPIPGTRAW_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file W32NamedPipesGatesRaw.h
\brief Gates for named pipes - raw (w/o framework) version.

*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include <string>
#include <ctime>
#include <memory>

#include "windows.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class grdW32NamedPipesGateInServer;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

/// Default limit of messages for input queue 
const unsigned int W32NP_DEF_INPUT_QUEUE_LIMIT = 100000;
/// Default timeout [ms]
const unsigned int W32NP_DEF_TIMEOUT = 5000;
/// Default connection timeout on server side [ms]
const unsigned int W32NP_CONN_TIMEOUT_SRV = W32NP_DEF_TIMEOUT;
/// Default pipe timeout [ms]
const unsigned int W32NP_PIPE_TIMEOUT_SRV = W32NP_DEF_TIMEOUT;
/// Default message wait timemeout (after connection has been established) [ms]
const unsigned int W32NP_MSG_WAIT_TIMEOUT = W32NP_DEF_TIMEOUT;
/// Default message size limit
const unsigned int W32NP_MSG_SIZE_LIMIT = 64*1024;
/// Indicates queue is full and gate cannot load anything more
const unsigned int W32NP_QUEUE_FULL = ((unsigned int)-1);
/// Time (ms) for sleep when input queue is full
const unsigned int W32NP_BUSY_SLEEP = 10;
/// Time (ms) for sleep when input queue is empty or cannot be connected
const unsigned int W32NP_WAIT_SLEEP = 1;

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

/// Log interface
class grdW32NamedPipesLogIntf {
public:
  virtual ~grdW32NamedPipesLogIntf() {}
  virtual void writeError(const std::string &errorMsg, unsigned int errorCode) = 0;
  virtual void writeWarning(const std::string &message) = 0;
  virtual void writeDebug(const std::string &message) = 0;
};

/// Abstract background processing worker interface
class grdW32NamedPipesInputWorkerIntf { 
public:
  virtual ~grdW32NamedPipesInputWorkerIntf() {}
  /// Prepare worker for background processing 
  virtual void start() = 0;
  /// Execute one step of background processing
  virtual unsigned int run() = 0;
  /// Stop background processing
  virtual void stop() = 0;
  /// Returns {true} if background processing is running
  virtual bool isRunning() = 0;
};

/// Abstract message interface
class grdW32NamedPipesMessageIntf {
public:
  virtual ~grdW32NamedPipesMessageIntf() {}
  virtual void setValue(const char *data, size_t dataSize) = 0;
  virtual void setValue(grdW32NamedPipesMessageIntf &value) = 0;
  virtual void getValue(char *buffer, size_t bufferSize, size_t &returnedSize) const = 0;
  virtual void getValue(grdW32NamedPipesMessageIntf &output) const = 0;
  /// Prepare exact copy of message 
  virtual grdW32NamedPipesMessageIntf *clone() const = 0;
  /// Clear contents
  virtual void clear() = 0;
  /// Returns minimal size of buffer required for storing contents of message as (char *)
  virtual size_t getDataSize() const = 0;
};

/// Message which stores one value - string
class grdW32NamedPipesStringMessage: public grdW32NamedPipesMessageIntf {
public:
  grdW32NamedPipesStringMessage();
  grdW32NamedPipesStringMessage(const std::string &data);
  virtual ~grdW32NamedPipesStringMessage();
  virtual void setValue(const std::string &data);
  virtual void setValue(const char *data, size_t dataSize);
  virtual void setValue(grdW32NamedPipesMessageIntf &value);
  virtual void getValue(std::string &output) const;
  virtual void getValue(char *buffer, size_t bufferSize, size_t &returnedSize) const;
  virtual void getValue(grdW32NamedPipesMessageIntf &output) const;
  virtual grdW32NamedPipesMessageIntf *clone() const;
  virtual void clear();
  virtual size_t getDataSize() const;
private:
  std::string m_data;
};

/// Abstract message queue interface
class grdW32NamedPipesQueueIntf {
public:
  virtual ~grdW32NamedPipesQueueIntf() {}
  /// Returns number of messages in queue
  virtual size_t size() const = 0;
  /// Returns {true} if queue is empty
  virtual bool empty() const = 0;
  /// Add message to end of queue
  virtual void put(const grdW32NamedPipesMessageIntf &message) = 0;
  /// Returns first message from queue - if available - and remove it from queue
  /// @return {true} - when message has been found
  virtual bool tryGet(grdW32NamedPipesMessageIntf &message) = 0;
  /// Returns first message from queue - if available - and keep message in queue
  /// @return {true} - when message has been found
  virtual bool peek(grdW32NamedPipesMessageIntf &message) = 0;
  /// Removes first message from queue
  virtual void eraseTop() = 0;
};

/// Input gate
class grdW32NamedPipesGateInRaw: public grdW32NamedPipesInputWorkerIntf {
public:
  // construction
  grdW32NamedPipesGateInRaw();
  virtual ~grdW32NamedPipesGateInRaw();
  // properties
  void setPipePath(const std::string &aPath);
  std::string getPipePath();
  void setPipeLimit(unsigned int value);
  void setLog(grdW32NamedPipesLogIntf *log);
  void setMessageQueue(grdW32NamedPipesQueueIntf *queue);
  void setMessagePrototype(grdW32NamedPipesMessageIntf *message);
  // process
  virtual bool hasMessage();
  virtual void getMessage(grdW32NamedPipesMessageIntf &outMsg);
  virtual void start();
  virtual unsigned int run();
  virtual void stop();
  virtual bool isRunning();
protected:
  void checkPrepared();
  void prepare();
  bool isPrepared();
  virtual void handleError(const std::string &errorMsg, unsigned int errorCode = 0);
  virtual void handleWarning(const std::string &message);
  virtual void handleDebug(const std::string &message);
private:
private:
  unsigned int m_messageLimit;
  unsigned int m_pipeLimit;
  bool m_throwOnErrorEnabled;
  grdW32NamedPipesGateInServer *m_inputServer;
  std::string m_pipePath;
  grdW32NamedPipesLogIntf *m_log;
  grdW32NamedPipesQueueIntf *m_messageQueue;
  grdW32NamedPipesMessageIntf *m_messagePrototype;
};

/// Output gate
class grdW32NamedPipesGateOutRaw { 
public:
  grdW32NamedPipesGateOutRaw();
  ~grdW32NamedPipesGateOutRaw();
  virtual void putMessage(const grdW32NamedPipesMessageIntf &aMsg);
  virtual unsigned int run();
  bool isConnected();
  void setPipePath(const std::string &aPath);
  std::string getPipePath();
  void setLog(grdW32NamedPipesLogIntf *log);
  void setMessageQueue(grdW32NamedPipesQueueIntf *queue);
  void setMessagePrototype(grdW32NamedPipesMessageIntf *message);
protected:
  void connect();
  void disconnect();
  void reconnect();
  bool sendMessage(const grdW32NamedPipesMessageIntf &aMsg);
  bool enqueueMessage(const grdW32NamedPipesMessageIntf &aMsg);
  bool sendWithRetry(const grdW32NamedPipesMessageIntf &aMsg);
  bool isQueueEmpty();
  void sendEof();
  void flush();
  virtual void handleError(const std::string &errorMsg, unsigned int errorCode = 0);
  virtual void handleWarning(const std::string &message);
  virtual void handleDebug(const std::string &message);
  void prepareBuffer(size_t newSize);
private:
  bool m_connected;
  bool m_throwOnErrorEnabled;
  unsigned int m_connectTimeout;
  /// Maximum number of output messages that can be stored in queue, error on overflow
  unsigned int m_messageQueueLimit;
  /// Maximum number of output messages that can be buffered before automatic send
  unsigned int m_messageBufferLimit;
  unsigned int m_maxSendDelay;
  clock_t m_lastSendTime;
  grdW32NamedPipesLogIntf *m_log;
  HANDLE m_pipeHandle;
  std::string m_pipePath;
  grdW32NamedPipesQueueIntf *m_messageQueue;
  grdW32NamedPipesMessageIntf *m_messagePrototype;
  std::auto_ptr<grdW32NamedPipesMessageIntf> m_emptyMessage;
  char *m_buffer;
  size_t m_bufferSize;
  size_t m_sessionMessageLimit;
};

/// Module default error class
class grdW32NamedPipesRawError: public std::runtime_error {
public:
  grdW32NamedPipesRawError(const std::string &msg): std::runtime_error(msg.c_str()) {}
};

#endif // _W32NAMPIPGTRAW_H__