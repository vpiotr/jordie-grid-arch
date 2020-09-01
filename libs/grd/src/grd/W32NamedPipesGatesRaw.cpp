/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesGateRaw.cpp
// Project:     grdLib
// Purpose:     Gates for named pipes - raw (w/o framework) version.
// Author:      Piotr Likus
// Modified by:
// Created:     28/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////
// see http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancednamedpipe15a.html

#include <sstream>
#include <cassert>
#include <windows.h> 

//#include "sc\proc\process.h"

#include "grd/W32NamedPipesGatesRaw.h"

#define PIPE_STATE_CONNECTING 1 
#define PIPE_STATE_READING 2 
#define PIPE_STATE_WRITING 3 
//#define INSTANCES 4 
//#define W32NP_CONN_TIMEOUT_SRV

#define W32NP_MSG_TIMEOUT ((DWORD)-1)
#define W32NP_SLEEP_ON_SUCCESS 1
#define W32NP_READ_TIMEOUT_SRV 500

// define to reconnect on each message
//#define W32NP_RECONNECT_ON_EACH_MSG

//#define DEBUG_LOG_CONNECTIONS
//#define DEBUG_LOG_CONNECTIONS_WR

// ----------------------------------------------------------------------------
// local functions
// ----------------------------------------------------------------------------
std::string getWin32ErrorMsg(DWORD errorCd)
{
  std::string res;

    if (errorCd != 0) {
        LPVOID lpMsgBuf;

        FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCd,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

        std::string msg((LPCTSTR)lpMsgBuf);
        res = msg;

        LocalFree(lpMsgBuf);
    }
  return res;
}

bool isEofMessage(char *cptr, size_t len)
{
  if (!len || !*cptr)
    return true;
  else 
    return false;
}

template <typename T> 
std::string toString(const T &arg) {
    std::ostringstream	out;
    out << arg;
    return(out.str());
}

void throwW32NamedPipesRawError(const std::string &msg, DWORD errorCd)
{
  throw grdW32NamedPipesRawError(msg+", error code: "+toString(errorCd)+", message: "+getWin32ErrorMsg(errorCd)); 
}

clock_t clock_time_ms()
{
  clock_t ticksPerMs = CLOCKS_PER_SEC / 1000;
  return clock() / ticksPerMs;
}

bool is_time_elapsed(clock_t startTime, clock_t delay)
{
  clock_t currentTime = clock_time_ms();
  if (startTime <= currentTime)
    return (currentTime - startTime >= delay);
  else
    return true;
}

// ----------------------------------------------------------------------------
// local classes & structures
// ----------------------------------------------------------------------------
typedef struct 
{ 
   OVERLAPPED oOverlap; 
   HANDLE hPipeInst; 
   TCHAR chRequest[W32NP_MSG_SIZE_LIMIT]; 
   DWORD cbRead;
   TCHAR chReply[W32NP_MSG_SIZE_LIMIT];
   DWORD cbToWrite; 
   DWORD dwState; 
   BOOL fPendingIO; 
} PIPEINST, *LPPIPEINST; 

class grdW32NamedPipesGateInServer {
public:
  // construction
  grdW32NamedPipesGateInServer();
  virtual ~grdW32NamedPipesGateInServer();
  // properties
  void setPipePath(const std::string &value);
  std::string getPipePath();
  void setLog(grdW32NamedPipesLogIntf *log);
  // process
  virtual unsigned int run();
  virtual void start();
  virtual void stop();
  bool isStopped();
protected:
  void checkPrepared();
  bool isPrepared();
  void prepare();
  void unprepare();
  //VOID DisconnectAndReconnect(DWORD pipeNo);
  //BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo);
  virtual void handleMessage(TCHAR *msg, DWORD msgLen) = 0;
  virtual bool canReadMessages();
  void tryHandleMessage(TCHAR *msg, DWORD msgLen);
  void connectClientBlocking();
  bool connectClientNonBlocking();
  DWORD waitForMessage();
  bool initConnection();
  void disconnect();
  bool isConnected();
  void handleError(const std::string &errorMsg, unsigned int errorCode = 0);
  void handleWarning(const std::string &message);
  void handleDebug(const std::string &message);
private:
  bool m_prepared;
  bool m_connected;
  bool m_stopped;
  bool m_throwOnErrorEnabled;
  bool m_pendingIo;
  HANDLE m_pipeHandle;  
  OVERLAPPED m_overlapped;
  std::string m_pipePath;
  char *m_dataBuffer;
  DWORD m_pipeState;
  grdW32NamedPipesLogIntf *m_log;
};

class grdLocalNamedPipeServerWithQueue: public grdW32NamedPipesGateInServer {
public:
  grdLocalNamedPipeServerWithQueue(grdW32NamedPipesQueueIntf &queue, grdW32NamedPipesMessageIntf &messagePrototype)
    : grdW32NamedPipesGateInServer(), m_queue(&queue), m_messagePrototype(&messagePrototype)
  {}
  virtual ~grdLocalNamedPipeServerWithQueue() {}
  void setMessageLimit(unsigned int value) { m_messageLimit = value; }
  void setSessionMessageLimit(unsigned int value) { m_sessionMessageLimit = value; }
protected:
  virtual void handleMessage(TCHAR *msg, DWORD msgLen) {
    assert(m_queue != NULL);
    assert(m_messagePrototype != NULL);

    std::auto_ptr<grdW32NamedPipesMessageIntf> msgData(m_messagePrototype->clone());
    msgData->setValue(msg, msgLen);

    if (m_queue->size() < m_messageLimit)
      m_queue->put(*msgData);
    else
      throw grdW32NamedPipesRawError("Input message limit reached in server");
  }
  virtual bool canReadMessages() {
    assert(m_queue != NULL);
    size_t currSize = m_queue->size();

    if (m_messageLimit > 0)
    {
      return (currSize + m_sessionMessageLimit <= m_messageLimit);
    } else {
      return true;
    }
  }
private:
  grdW32NamedPipesQueueIntf *m_queue;
  grdW32NamedPipesMessageIntf *m_messagePrototype;
  unsigned int m_messageLimit;
  unsigned int m_sessionMessageLimit;
};

// ----------------------------------------------------------------------------
// grdW32NamedPipesGateInServer
// ----------------------------------------------------------------------------
grdW32NamedPipesGateInServer::grdW32NamedPipesGateInServer()
{
  m_stopped = true;
  m_prepared = false;
  m_connected = false;
  m_pendingIo = false;
  m_throwOnErrorEnabled = true;
  m_dataBuffer = NULL;
  m_pipeState = 0;
}

grdW32NamedPipesGateInServer::~grdW32NamedPipesGateInServer()
{
  unprepare();
  //delete []m_pipes;
  //delete []m_events;
  delete []m_dataBuffer;
}

void grdW32NamedPipesGateInServer::setPipePath(const std::string &value)
{
  m_pipePath = value;
}

std::string grdW32NamedPipesGateInServer::getPipePath()
{
  return m_pipePath;
}

void grdW32NamedPipesGateInServer::setLog(grdW32NamedPipesLogIntf *log)
{
  m_log = log;
}

bool grdW32NamedPipesGateInServer::canReadMessages() 
{
  return true;
}

bool grdW32NamedPipesGateInServer::isPrepared()
{
  return m_prepared;
}

void grdW32NamedPipesGateInServer::prepare()
{
//  if (m_pipeLimit == 0)
//    throw grdW32NamedPipesRawError("Pipe limit not defined");

  OVERLAPPED ol = {0,0,0,0,NULL};
  m_overlapped = ol;
  m_overlapped.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

  if (m_overlapped.hEvent == NULL) 
  {
       DWORD errCd = GetLastError();
       //throwW32NamedPipesRawError(std::string("CreateEvent failed"), errCd);
       handleError(std::string("CreateEvent failed"), errCd);
  }

  m_pipeHandle = CreateNamedPipe( 
         m_pipePath.c_str(),            // pipe name 
         PIPE_ACCESS_INBOUND|       // read/write access 
         FILE_FLAG_OVERLAPPED,
         PIPE_TYPE_MESSAGE |      // message-type pipe 
         PIPE_READMODE_MESSAGE |  // message-read mode 
         PIPE_WAIT,               // blocking mode 
         PIPE_UNLIMITED_INSTANCES,               // number of instances 
         W32NP_MSG_SIZE_LIMIT*sizeof(TCHAR),   // output buffer size 
         W32NP_MSG_SIZE_LIMIT*sizeof(TCHAR),   // input buffer size 
         W32NP_PIPE_TIMEOUT_SRV,               // client time-out 
         NULL);                   // default security attributes 

  if (m_pipeHandle == INVALID_HANDLE_VALUE) 
  {
     DWORD errCd = GetLastError();
     handleError(std::string("CreateNamedPipe failed"), errCd);
  }

  m_dataBuffer = new char[W32NP_MSG_SIZE_LIMIT * sizeof(TCHAR)];
  m_prepared = true;
  m_connected = false;
  initConnection();
}

void grdW32NamedPipesGateInServer::unprepare()
{
  disconnect();
  CloseHandle(m_pipeHandle); 
  CloseHandle(m_overlapped.hEvent);
  m_prepared = false;
}

bool grdW32NamedPipesGateInServer::isStopped()
{
  return m_stopped;
}

void grdW32NamedPipesGateInServer::start()
{
  m_stopped = false;
  checkPrepared();
}

void grdW32NamedPipesGateInServer::stop()
{
  m_stopped = true;
}

void grdW32NamedPipesGateInServer::checkPrepared()
{
   if (!isPrepared())
   {
     prepare();
     m_stopped = false;
   }
}

void grdW32NamedPipesGateInServer::connectClientBlocking()
{
   ConnectNamedPipe(m_pipeHandle, NULL);
}

bool grdW32NamedPipesGateInServer::connectClientNonBlocking()
{
  BOOL fPendingIO = FALSE;
 
  BOOL fConnected = ConnectNamedPipe(m_pipeHandle, &m_overlapped);

  DWORD errCd;

  if (fConnected) {
    errCd = GetLastError();
    handleError("Connect client", errCd);
  }
 
  errCd = GetLastError();
  switch( errCd ) {
        case ERROR_PIPE_CONNECTED:
          if (SetEvent(m_overlapped.hEvent))
            break;
          handleError("Connected error", errCd);
          break;
      case ERROR_IO_PENDING:
          fPendingIO = TRUE;
          break;
      default:
          //throwW32NamedPipesRawError(std::string("connect client failed"), errCd);
          handleError(std::string("connect client failed"), errCd);
          break;
  }
 
  return (fPendingIO == TRUE);
}

bool grdW32NamedPipesGateInServer::initConnection()
{
   if (m_connected)
     return true;

   m_pendingIo = connectClientNonBlocking();
   m_pipeState = m_pendingIo ? PIPE_STATE_CONNECTING : PIPE_STATE_READING; 

   bool waitSuccess = (WaitForSingleObject(m_overlapped.hEvent, W32NP_CONN_TIMEOUT_SRV) != WAIT_TIMEOUT);
   if (!waitSuccess) {
     handleDebug("Server - wait for client timeout");
     if (m_pendingIo) {
       CancelIo(m_pipeHandle);
       m_pendingIo = false;
       m_pipeState = 0;
     }
     return false;
   }

   m_connected = true;
   return true;
}

void grdW32NamedPipesGateInServer::disconnect()
{
  DisconnectNamedPipe(m_pipeHandle);
  m_connected = false;
}

bool grdW32NamedPipesGateInServer::isConnected()
{
  return m_connected;
}

// returns number of handled messages
unsigned int grdW32NamedPipesGateInServer::run()
{
   unsigned int res = 0;
   checkPrepared();

   DWORD i, dwWait, cbRet, dwErr, cbBytesRead; 
   BOOL fSuccess, fPendingIo; 
   std::string errorMsg;
 
   //if (m_pendingIo) {
   //  CancelIo(m_pipeHandle);
   //  m_pendingIo = false;
   //  m_pipeState = 0;
   //}

   if (!canReadMessages())
     return W32NP_QUEUE_FULL;

   if (!isConnected())
     initConnection();

   if (!isConnected())
     return 0;

#ifdef DEBUG_LOG_CONNECTIONS
   handleDebug("Client connected");
#endif

    // wait for message
    cbBytesRead = waitForMessage();

    if (cbBytesRead == W32NP_MSG_TIMEOUT)
    {
      disconnect();
      return 0;
    }

    // handle all found messages from client
    while(!isStopped()) //(cbBytesRead > 0))
    {
      fSuccess = ReadFile(m_pipeHandle,                       // handle to pipe 
                      m_dataBuffer,                           // buffer to receive data 
                      W32NP_MSG_SIZE_LIMIT*sizeof(TCHAR) - 1, // size of buffer 
                      &cbBytesRead,                           // number of bytes read 
                      &m_overlapped);                         // overlapped I/O 

      fPendingIo = FALSE;

      if (!fSuccess || cbBytesRead == 0) 
      {
        dwErr = GetLastError(); 
        if (!fSuccess && (dwErr == ERROR_IO_PENDING)) 
        { 
          fPendingIo = TRUE; 
        } else {
          handleError("ReadFile error", dwErr);
        }
      }

      if (fPendingIo)
        fSuccess = (WaitForSingleObject(m_overlapped.hEvent, W32NP_READ_TIMEOUT_SRV) != WAIT_TIMEOUT)?TRUE:FALSE;

      if ((fSuccess != FALSE) && fPendingIo)
      {
         fSuccess = GetOverlappedResult( 
            m_pipeHandle, // handle to pipe 
            &m_overlapped, // OVERLAPPED structure 
            &cbBytesRead,  // bytes transferred 
            TRUE);        // wait 
      }

      if (fSuccess != FALSE)
      {
          m_dataBuffer[cbBytesRead] = '\0';

          if (isEofMessage(m_dataBuffer, cbBytesRead+1))
            break;

          tryHandleMessage(m_dataBuffer, cbBytesRead + 1);
          res++;
      }

      // test reconnect-on-every-request
#ifdef W32NP_RECONNECT_ON_EACH_MSG
      break;
#endif
      if (fSuccess == FALSE)
        break;
      
      //--------------------------------------------------------
      //Disabled - could cause break on message not-ready-yet
      //--------------------------------------------------------
      //if (fSuccess != FALSE)
      //  PeekNamedPipe(m_pipeHandle,NULL,0,NULL,&cbBytesRead,NULL);
      //else
      //  break;
    }

#ifdef DEBUG_LOG_CONNECTIONS
    handleDebug("disconnecting client");
#endif

    // test reconnect-on-every-request
    disconnect();
#ifdef DEBUG_LOG_CONNECTIONS
    handleDebug("Client disconnected");
#endif
 
  return res; 
} 

DWORD grdW32NamedPipesGateInServer::waitForMessage()
{
  DWORD cbBytesRead = 0;
  clock_t startTime = clock_time_ms();

  while(!isStopped() && (cbBytesRead == 0))
  {
    if (is_time_elapsed(startTime, W32NP_MSG_WAIT_TIMEOUT))
    {
      cbBytesRead = W32NP_MSG_TIMEOUT;
      break;
    }
    PeekNamedPipe(m_pipeHandle,NULL,0,NULL,&cbBytesRead,NULL);
  }

  return cbBytesRead;
}

void grdW32NamedPipesGateInServer::tryHandleMessage(TCHAR *msg, DWORD msgLen)
{
    try {
      handleMessage(msg, msgLen);
    } 
    catch(grdW32NamedPipesRawError &e) {
      std::string errorMsg = std::string("grdW32NamedPipesRawError in message handler: ")+std::string(e.what());
      handleWarning(errorMsg);
    }
    catch(const std::exception& e) {
      std::string errorMsg = std::string("exception in message handler: ")+std::string(e.what());
      handleWarning(errorMsg);
    }
    catch(...) {
      std::string errorMsg = "Undefined error in message handler";
      handleWarning(errorMsg);
    }
}

void grdW32NamedPipesGateInServer::handleError(const std::string &errorMsg, unsigned int errorCode)
{
  if (m_throwOnErrorEnabled)
    throwW32NamedPipesRawError(errorMsg, errorCode);
  else if (m_log != NULL)
    m_log->writeError(errorMsg, errorCode);
}

void grdW32NamedPipesGateInServer::handleWarning(const std::string &message)
{
  if (m_log != NULL)
    m_log->writeWarning(message);
}

void grdW32NamedPipesGateInServer::handleDebug(const std::string &message)
{
  if (m_log != NULL)
    m_log->writeDebug(message);
}

// ----------------------------------------------------------------------------
// grdW32NamedPipesGateInRaw
// ----------------------------------------------------------------------------
grdW32NamedPipesGateInRaw::grdW32NamedPipesGateInRaw()
{
  m_messageLimit = W32NP_DEF_INPUT_QUEUE_LIMIT;
  m_throwOnErrorEnabled = true;
  m_log = NULL;
  m_messageQueue = NULL;
  m_messagePrototype = NULL;
}

grdW32NamedPipesGateInRaw::~grdW32NamedPipesGateInRaw()
{
  delete m_inputServer;
}

void grdW32NamedPipesGateInRaw::setPipePath(const std::string &aPath)
{
  m_pipePath = aPath;
}

std::string grdW32NamedPipesGateInRaw::getPipePath()
{
  return m_pipePath;
}

void grdW32NamedPipesGateInRaw::setPipeLimit(unsigned int value)
{
  m_pipeLimit = value;
}

void grdW32NamedPipesGateInRaw::setLog(grdW32NamedPipesLogIntf *log)
{
  m_log = log;
}

void grdW32NamedPipesGateInRaw::setMessageQueue(grdW32NamedPipesQueueIntf *queue)
{
  m_messageQueue = queue;
}

void grdW32NamedPipesGateInRaw::setMessagePrototype(grdW32NamedPipesMessageIntf *message)
{
  m_messagePrototype = message;
}

void grdW32NamedPipesGateInRaw::checkPrepared()
{
  if (!isPrepared())
    prepare();
}

bool grdW32NamedPipesGateInRaw::isPrepared()
{
  return (m_inputServer != NULL);
}

void grdW32NamedPipesGateInRaw::prepare()
{
  if (m_pipePath.empty())
  {
    handleError("Pipe path required");
    return;
  }

  if (m_messageQueue == NULL)
  {
    handleError("Message queue not assigned");
    return;
  }

  if (m_messagePrototype == NULL)
  {
    handleError("Message prototype not assigned");
    return;
  }

  std::auto_ptr<grdLocalNamedPipeServerWithQueue> inputServer(
    new grdLocalNamedPipeServerWithQueue(*m_messageQueue, *m_messagePrototype));
  inputServer->setMessageLimit(m_messageLimit);
  inputServer->setSessionMessageLimit(m_messageLimit / 2);
  //inputServer->setPipeLimit(m_pipeLimit);
  inputServer->setPipePath(m_pipePath);
  inputServer->setLog(m_log);
  m_inputServer = inputServer.release();
}

bool grdW32NamedPipesGateInRaw::hasMessage()
{
  checkPrepared();
  return !m_messageQueue->empty();
}

void grdW32NamedPipesGateInRaw::getMessage(grdW32NamedPipesMessageIntf &outMsg) 
{
  checkPrepared();
  if (!m_messageQueue->tryGet(outMsg)) {
    handleError("Input queue empty");
    outMsg.clear();
    return;
  }
}

// returns number of read messages 
unsigned int grdW32NamedPipesGateInRaw::run()
{
  return m_inputServer->run();
}

// prepare for background processing
void grdW32NamedPipesGateInRaw::start()
{
  prepare();
  m_inputServer->start();
}

bool grdW32NamedPipesGateInRaw::isRunning()
{
  return !m_inputServer->isStopped();
}

// request stop of background processing
void grdW32NamedPipesGateInRaw::stop()
{
  m_inputServer->stop();
}

void grdW32NamedPipesGateInRaw::handleError(const std::string &errorMsg, unsigned int errorCode)
{
  if (m_throwOnErrorEnabled)
    throwW32NamedPipesRawError(errorMsg, errorCode);
  else if (m_log != NULL)
    m_log->writeError(errorMsg, errorCode);
}

void grdW32NamedPipesGateInRaw::handleWarning(const std::string &message)
{
  if (m_log != NULL)
    m_log->writeWarning(message);
}

void grdW32NamedPipesGateInRaw::handleDebug(const std::string &message)
{
  if (m_log != NULL)
    m_log->writeDebug(message);
}

// ----------------------------------------------------------------------------
// grdW32NamedPipesGateOutRaw
// ----------------------------------------------------------------------------
grdW32NamedPipesGateOutRaw::grdW32NamedPipesGateOutRaw()
{
  m_connected = false;
  m_connectTimeout = 20000;
  m_messageQueueLimit = 0; //100000;
  m_messageBufferLimit = 0;

#ifdef W32NP_SLEEP_ON_SUCCESS
  //m_maxSendDelay = 10; //200; // 2000
  m_maxSendDelay = 0; 
#else
  m_maxSendDelay = 0; //200; // 2000
#endif

  m_lastSendTime = clock_time_ms();
  m_log = NULL;
  m_throwOnErrorEnabled = true;
  m_messageQueue = NULL;
  m_messagePrototype = NULL;
  m_buffer = NULL;
  m_bufferSize = 0;

  m_sessionMessageLimit = 100;
}

grdW32NamedPipesGateOutRaw::~grdW32NamedPipesGateOutRaw()
{
  if (isConnected())
    disconnect();
  if (m_buffer != NULL)
    delete []m_buffer;
}

void grdW32NamedPipesGateOutRaw::setLog(grdW32NamedPipesLogIntf *log)
{
  m_log = log;
}

void grdW32NamedPipesGateOutRaw::setMessageQueue(grdW32NamedPipesQueueIntf *queue)
{
  m_messageQueue = queue;
}

void grdW32NamedPipesGateOutRaw::setMessagePrototype(grdW32NamedPipesMessageIntf *message)
{
  m_messagePrototype = message;
  if (message != NULL) {
    m_emptyMessage.reset(message->clone());
    m_emptyMessage->clear();
  } else {
    m_emptyMessage.reset();
  }
}

void grdW32NamedPipesGateOutRaw::setPipePath(const std::string &aPath)
{
  m_pipePath = aPath;
}

std::string grdW32NamedPipesGateOutRaw::getPipePath()
{
  return m_pipePath;
}

void grdW32NamedPipesGateOutRaw::putMessage(const grdW32NamedPipesMessageIntf &aMsg)
{
   enqueueMessage(aMsg);
}

bool grdW32NamedPipesGateOutRaw::sendWithRetry(const grdW32NamedPipesMessageIntf &aMsg)
{
  bool success = true;

  if (!sendMessage(aMsg))
  {
    DWORD errCd = GetLastError();
    handleWarning("Client - write failed, error: "+toString(errCd)+", message: "+getWin32ErrorMsg(errCd));
    handleWarning("Performing reconnect & retry");
    reconnect();

    success = false;
    if (isConnected() && sendMessage(aMsg)) {
      success = true;
    }
  }

  return success;
}

bool grdW32NamedPipesGateOutRaw::isQueueEmpty()
{
  return m_messageQueue->empty();
}

bool grdW32NamedPipesGateOutRaw::enqueueMessage(const grdW32NamedPipesMessageIntf &aMsg)
{
  assert(m_messageQueue != NULL);
  if ((m_messageQueueLimit == 0) || (m_messageQueue->size() < m_messageQueueLimit)) {
    m_messageQueue->put(aMsg);
    return true;
  } else {
    handleError("Output message limit reached in server");
    return false;
  }
}

bool grdW32NamedPipesGateOutRaw::isConnected()
{
  return m_connected;
}

// returns number of sent messages
unsigned int grdW32NamedPipesGateOutRaw::run()
{
  assert(m_messageQueue != NULL);
  assert(m_messagePrototype != NULL);

  bool success;
  unsigned int msgCnt = 0;
  unsigned int sendErrCnt = 0;
  size_t msgCntInQueue;

  msgCntInQueue = m_messageQueue->size();

  if (!msgCntInQueue)
    return 0;

  bool forceSendByTime = false;

  if ((m_maxSendDelay > 0) && is_time_elapsed(m_lastSendTime, m_maxSendDelay))
    forceSendByTime = true;

  bool forceBySessionLen = false;

  if ((m_sessionMessageLimit > 0) && (msgCntInQueue >= m_sessionMessageLimit))
    forceBySessionLen = true;

  if (!forceSendByTime && !forceBySessionLen && (m_messageBufferLimit > 0) && (msgCntInQueue < m_messageBufferLimit))
    return 0;

  if (!isConnected())
    connect();

  if (!isConnected())
    return 0;

  std::auto_ptr<grdW32NamedPipesMessageIntf> messageData(m_messagePrototype->clone());

  do
  {
    success = m_messageQueue->peek(*messageData);
    if (success)
    {
      success = sendMessage(*messageData);
      if (success) {
        m_messageQueue->eraseTop(); // erase element
        msgCnt++;
#ifdef W32NP_RECONNECT_ON_EACH_MSG
        disconnect();
        break;
#endif
      } else {
        sendErrCnt++;
      }
    }

    if ((m_sessionMessageLimit > 0) && (msgCnt >= m_sessionMessageLimit))
      break;
  } while(success);

  if (!isConnected())
    return msgCnt;

  if (sendErrCnt) {
    disconnect();
    return msgCnt;
  }

  if (msgCnt > 0)
  {
    m_lastSendTime = clock_time_ms();
    sendEof();
    flush();
    disconnect();
  }

  return msgCnt;
}

void grdW32NamedPipesGateOutRaw::sendEof()
{
  sendMessage(*m_emptyMessage);
}

void grdW32NamedPipesGateOutRaw::flush()
{
  //BOOL fSuccess = 
  FlushFileBuffers(m_pipeHandle); 

  //if (!fSuccess) 
  //{
  //  DWORD errCd = GetLastError();
  //  handleError(std::string("Flush failed"), errCd);
  //  return;
  //}
}

void grdW32NamedPipesGateOutRaw::prepareBuffer(size_t newSize)
{
  if (m_bufferSize <= newSize)
  {
    if (m_buffer != NULL)
      delete []m_buffer;
    size_t newBufferSize = 15 * newSize / 10;
    m_buffer = new char[newBufferSize];
    m_bufferSize = newBufferSize;
  }
}

bool grdW32NamedPipesGateOutRaw::sendMessage(const grdW32NamedPipesMessageIntf &aMsg)
{
   prepareBuffer(aMsg.getDataSize()); 

   size_t readSize;
   aMsg.getValue(m_buffer, m_bufferSize, readSize);

   DWORD cbToWrite = readSize;
   DWORD cbWritten;
   bool retry = false;
   BOOL fSuccess;
   DWORD errCd;

   fSuccess = WriteFile( 
      m_pipeHandle,           // pipe handle 
      m_buffer,               // message 
      cbToWrite,              // message length 
      &cbWritten,             // bytes written 
      NULL);                  // not overlapped 

   if (!fSuccess)
     return false;

   //if (!fSuccess)
   //{
   //   throwW32NamedPipesRawError(std::string("WriteFile to pipe failed."), errCd); 
   //}

   //fSuccess = 

/*
   if (!fSuccess)
   {
      DWORD errCd = GetLastError();
      throwW32NamedPipesRawError(std::string("Flush pipe failed."), errCd); 
   }
*/
   return true;
}

void grdW32NamedPipesGateOutRaw::connect()
{
  if (m_connected)
    return;

   DWORD  cbRead, cbToWrite, cbWritten, dwMode; 

   if (m_pipePath.empty()) {
     handleError("Pipe path required");
     return;
   }

   if (!WaitNamedPipe(m_pipePath.c_str(), m_connectTimeout)) 
   { 
     handleWarning("Pipe connect - wait timeout."); 
     return;
   } 

   while (1) 
   { 
      m_pipeHandle = CreateFile( 
         m_pipePath.c_str(),   // pipe name 
         GENERIC_WRITE,  // write access 
         0,              // no sharing 
         NULL,           // default security attributes
         OPEN_EXISTING,  // opens existing pipe 
         0,              // default attributes 
         NULL);          // no template file 
 
   // Break if the pipe handle is valid. 
 
      if (m_pipeHandle != INVALID_HANDLE_VALUE) 
         break; 
 
      // Exit if an error other than ERROR_PIPE_BUSY occurs. 
 
      DWORD errCd = GetLastError();
      if (errCd != ERROR_PIPE_BUSY) {
         handleError(std::string("Could not open pipe"), errCd);
         break;
      }
 
      // All pipe instances are busy, so wait for 20 seconds. 
 
      if (!WaitNamedPipe(m_pipePath.c_str(), m_connectTimeout)) 
      { 
         handleError("Pipe wait timeout."); 
         break;
      } 
   } 

   if (m_pipeHandle == INVALID_HANDLE_VALUE) 
     return;

// The pipe connected; change to message-read mode. 
 
   dwMode = PIPE_READMODE_MESSAGE; 

   BOOL fSuccess = SetNamedPipeHandleState( 
      m_pipeHandle,    // pipe handle 
      &dwMode,  // new pipe mode 
      NULL,     // don't set maximum bytes 
      NULL);    // don't set maximum time 

   if (!fSuccess) 
   {
      DWORD errCd = GetLastError();
      handleError(std::string("SetNamedPipeHandleState failed"), errCd);
      return;
   }

#ifdef DEBUG_LOG_CONNECTIONS_WR
   handleDebug("Pipe connected");
#endif // DEBUG_LOG_CONNECTIONS

   m_connected = true;
}

void grdW32NamedPipesGateOutRaw::disconnect()
{
  if (!m_connected)
    return;

  CloseHandle(m_pipeHandle); 

#ifdef DEBUG_LOG_CONNECTIONS_WR
   handleDebug("Pipe disconnected");
#endif

  m_connected = false;
}

void grdW32NamedPipesGateOutRaw::reconnect()
{
  disconnect();
  connect();
}

void grdW32NamedPipesGateOutRaw::handleError(const std::string &errorMsg, unsigned int errorCode)
{
  if (m_throwOnErrorEnabled)
    throwW32NamedPipesRawError(errorMsg, errorCode);
  else if (m_log != NULL)
    m_log->writeError(errorMsg, errorCode);
}

void grdW32NamedPipesGateOutRaw::handleWarning(const std::string &message)
{
  if (m_log != NULL)
    m_log->writeWarning(message);
}

void grdW32NamedPipesGateOutRaw::handleDebug(const std::string &message)
{
  if (m_log != NULL)
    m_log->writeDebug(message);
}

// ----------------------------------------------------------------------------
// grdW32NamedPipesStringMessage
// ----------------------------------------------------------------------------
grdW32NamedPipesStringMessage::grdW32NamedPipesStringMessage()
{
}

grdW32NamedPipesStringMessage::grdW32NamedPipesStringMessage(const std::string &data): m_data(data)
{
}

grdW32NamedPipesStringMessage::~grdW32NamedPipesStringMessage()
{
}

void grdW32NamedPipesStringMessage::setValue(const std::string &data)
{
  m_data = data;
}

void grdW32NamedPipesStringMessage::setValue(const char *data, size_t dataSize)
{
  m_data = std::string(data, dataSize);
}

void grdW32NamedPipesStringMessage::setValue(grdW32NamedPipesMessageIntf &value)
{
  size_t bufferSize = value.getDataSize();
  if (!bufferSize)
    clear();
  else {
    size_t dataSize;
    char *bufferData = new char[bufferSize];

    try {
      value.getValue(bufferData, bufferSize, dataSize);
      m_data = std::string(bufferData, dataSize);
    }
    catch(...) {
      delete []bufferData;
      throw;
    }

    delete []bufferData;
  }
}

void grdW32NamedPipesStringMessage::getValue(std::string &output) const
{
  output = m_data;
}

void grdW32NamedPipesStringMessage::getValue(char *buffer, size_t bufferSize, size_t &returnedSize) const
{
  returnedSize = getDataSize();

  if (bufferSize < returnedSize)
    returnedSize = bufferSize;

  if (returnedSize > 0)
    strncpy(buffer, m_data.c_str(), returnedSize);
}

void grdW32NamedPipesStringMessage::getValue(grdW32NamedPipesMessageIntf &output) const
{
  output.setValue(m_data.c_str(), m_data.length() + 1);
}

grdW32NamedPipesMessageIntf *grdW32NamedPipesStringMessage::clone() const
{
  return new grdW32NamedPipesStringMessage(m_data);
}

void grdW32NamedPipesStringMessage::clear()
{
  m_data.clear();
}

size_t grdW32NamedPipesStringMessage::getDataSize() const
{
  return m_data.length() + 1;
}


