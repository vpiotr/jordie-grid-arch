/////////////////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesInputThread.h
// Project:     grdLib
// Purpose:     Thread for background input gate processing for Win32 Named Pipes gate
// Author:      Piotr Likus
// Modified by:
// Created:     05/10/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef _W32NPINTHREAD_H__
#define _W32NPINTHREAD_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file W32NamedPipesInputThread.h
\brief Thread for background input gate processing for Win32 Named Pipes gate

*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd\W32NamedPipesGatesRaw.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class grdLocalNamedPipeInputThreadWx;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

/// Abstract interface for input gate background processing
class grdW32NamedPipesInputThreadIntf {
public:
  /// Start processing inside thread 
  virtual int run() = 0;
  /// Stop all processing in thread 
  virtual void terminate() = 0;
};

/// Thread for background processing for input gate
class grdW32NamedPipesInputThread: public grdW32NamedPipesInputThreadIntf 
{
public:
  grdW32NamedPipesInputThread(grdW32NamedPipesInputWorkerIntf *worker);
  ~grdW32NamedPipesInputThread();
  virtual int run();
  virtual void terminate();
  virtual void setLog(grdW32NamedPipesLogIntf *log);
private:
  grdW32NamedPipesInputWorkerIntf *m_worker;
  grdLocalNamedPipeInputThreadWx *m_thread; // this will destroy itself on end
};


#endif // _W32NPINTHREAD_H__