/////////////////////////////////////////////////////////////////////////////
// Name:        WorkerTask.h
// Project:     scLib
// Purpose:     Task which handles asynchronymously a portion of work and
//              returns result. No persistency (restart) support.
//              
// Author:      Piotr Likus
// Modified by:
// Created:     05/05/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _WORKERTASK_H__
#define _WORKERTASK_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file WorkerTask.h
///
/// Use to perform work in parallel. 
/// For persistency (restart) use JobWorkerTask instead.
/// 
/// Example:
/// int MandelPartTaskB5::intRun()
///  {
///   if (isSleeping())
///     return 0;
/// 
///   scDataNode &params = getWorkParams(); 
///   uint stepSize = params.getInt("step_size", 5);
///   // perform work
///   // (..)
///   // on end post results
///   if (end-of-work) {
///     postResult(resultData);
///     requestStop();
///   }  
///   return 1;
/// }

    
// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/core.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scWorkerTask: public scTask {
public:
  scWorkerTask(const scEnvelope &requestEnvelope, scScheduler *scheduler);
  virtual ~scWorkerTask();
  scScheduler *getScheduler();
  virtual bool needsRun();
protected:  
  void postResult(const scDataNode &resultData);
  void postError(int code = 0);
  void postError(int code, const scDataNode &errorDetails);
  void postError(int code, const scString &details);
  const scDataNode &getWorkParams() const;
  void postResponse(const scResponse &response);
protected:
  scScheduler *m_scheduler;  
  scEnvelope m_reqEnvelope;
};

#endif // _WORKERTASK_H__