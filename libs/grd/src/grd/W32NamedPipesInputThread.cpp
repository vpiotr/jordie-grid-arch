/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesInputThread.cpp
// Project:     grdLib
// Purpose:     Thread for background input gate processing for Win32 Named Pipes gate
// Author:      Piotr Likus
// Modified by:
// Created:     05/10/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/W32NamedPipesInputThread.h"
#include "wx/thread.h"

static size_t gs_counter = (size_t)-1;
static wxCriticalSection gs_critsect;
static wxSemaphore gs_cond;

class grdLocalNamedPipeInputThreadWx: public wxThread
{
public:
  grdLocalNamedPipeInputThreadWx(grdW32NamedPipesInputWorkerIntf *worker): m_worker(worker), m_log(NULL), m_terminated(false) {
    Create();
  }

  void setLog(grdW32NamedPipesLogIntf *log) {
    m_log = log;
  }

  virtual ExitCode Entry() {
    std::string errorMsg;

    {
        wxCriticalSectionLocker lock(gs_critsect);
        if ( gs_counter == (size_t)-1 )
            gs_counter = 1;
        else
            gs_counter++;
    }

    try {
      unsigned int msgCnt;
      m_worker->start();
      while (!this->TestDestroy() && m_worker->isRunning())
      {
        msgCnt = m_worker->run();
        if (msgCnt == W32NP_QUEUE_FULL)
          this->Sleep(W32NP_BUSY_SLEEP);
        else if (msgCnt == 0)
          this->Sleep(W32NP_WAIT_SLEEP);
      }
    } 
    catch(grdW32NamedPipesRawError &e) {
      errorMsg = std::string("grdW32NamedPipesRawError: ")+std::string(e.what());
      handleWarning(errorMsg);
    }
    catch(const std::exception& e) {
      errorMsg = std::string("exception in input server: ")+std::string(e.what());
      handleWarning(errorMsg);
    }
    catch(...) {
      errorMsg = "Undefined error";
      handleWarning(errorMsg);
    }
    m_terminated = true;
    return 0;
  }

  virtual void OnExit() {
    m_terminated = true;
    wxCriticalSectionLocker lock(gs_critsect);
    if ( !--gs_counter )
        gs_cond.Post();
  }

  static void WaitForAll() {
    gs_cond.Wait();
  }

  bool isTerminated() { 
    return !m_worker->isRunning(); 
  }

  void handleWarning(const std::string &msg)
  {
    if (m_log != NULL)
      m_log->writeWarning(msg);
  }

private:
  grdW32NamedPipesInputWorkerIntf *m_worker;
  bool m_terminated;
  grdW32NamedPipesLogIntf *m_log;
};

// ----------------------------------------------------------------------------
// grdW32NamedPipesInputThread
// ----------------------------------------------------------------------------
grdW32NamedPipesInputThread::grdW32NamedPipesInputThread(grdW32NamedPipesInputWorkerIntf *worker) 
{
    m_worker = worker;
    m_thread = new grdLocalNamedPipeInputThreadWx(worker);
}

grdW32NamedPipesInputThread::~grdW32NamedPipesInputThread() 
{
}

void grdW32NamedPipesInputThread::setLog(grdW32NamedPipesLogIntf *log)
{
  m_thread->setLog(log);
}

int grdW32NamedPipesInputThread::run() 
{
  m_thread->Run();
  return 0;
}

void grdW32NamedPipesInputThread::terminate() 
{
  m_worker->stop();
  if (m_thread != NULL)
    m_thread->Delete();
}
