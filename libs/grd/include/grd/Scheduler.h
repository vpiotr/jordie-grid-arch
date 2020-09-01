/////////////////////////////////////////////////////////////////////////////
// Name:        Scheduler.h
// Project:     grdLib
// Purpose:     Public part of scheduler class interface.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDSCHEDULER_H__
#define _GRDSCHEDULER_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Scheduler.h
\brief Public part of scheduler class interface.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// base
#include "base/object.h"

// grd
#include "grd/Module.h"
#include "grd/Envelope.h"
#include "grd/RequestHandler.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
enum scSchedulerFeature {
  sfLogProcTime = 1,
  sfLogMessages = 2
};

enum scSchedulerStatus {
  ssCreated, 
  ssRunning, 
  ssStopping, 
  ssStopped
};

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scSchedulerIntf: public scInterface {
public:
  virtual ~scSchedulerIntf() {}
  // interface - properties
  virtual scString getName() const = 0;
  virtual void setName(const scString &a_name) = 0;
  virtual scSchedulerStatus getStatus() const = 0;
  virtual uint getNonDeamonTaskCount() = 0;
  // interface - architecture
  virtual void addModule(scModuleIntf *a_module) = 0;
  virtual void addTask(scTaskIntf *a_task) = 0;
  virtual void deleteTask(scTaskIntf *a_task) = 0;
  // interface - execution
  virtual void requestStop() = 0;
  virtual void run() = 0;
  virtual bool needsRun() = 0;
  // interface - address handling
  virtual scMessageAddress getOwnAddress(const scString &protocol = scString("")) = 0;
  /// Convert virtual address or alias to physical address
  virtual scString evaluateAddress(const scString &virtualAddr) = 0;
  // interface - message handling
  virtual scEnvelope *createErrorResponseFor(const scEnvelope &srcEnvelope, const scString &msg, int a_status) = 0;
  virtual void postMessage(const scString &address, const scString &command, 
    const scDataNode *params = SC_NULL, 
    int requestId = SC_REQUEST_ID_NULL,
    scRequestHandler *handler = SC_NULL) = 0;
  virtual void postEnvelopeForThis(scEnvelope *envelope) = 0;
  virtual void postEnvelope(scEnvelope *envelope, scRequestHandler *handler = SC_NULL) = 0;
  virtual void flushEvents() = 0;
  /// send message through dispatcher or this scheduler (if dispatcher not found)
  virtual bool forwardMessage(const scString &address, const scString &command, 
    const scDataNode *params, int requestId, scRequestHandler *handler) = 0;
};


#endif // _GRDSCHEDULER_H__