/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueClientImpl.h
// Project:     grdLib
// Purpose:     Implementation of work queue client interface.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _WORKQUEUECLIENTIMPL_H__
#define _WORKQUEUECLIENTIMPL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file WorkQueueClientImpl.h
///
/// File description

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/events/Events.h"
#include "grd/core.h"
#include "grd/WorkQueueClient.h"
#include "grd/details/SchedulerImpl.h"

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
class scWqServerProxyImpl: public scWqServerProxy {
public:
  scWqServerProxyImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget);
  virtual ~scWqServerProxyImpl() {}
  virtual scWqRequest *newRequest();
  virtual scWqRequestGroup *newGroup();
protected:
  scScheduler *m_scheduler; 
  scSignal *m_waitSignal; 
  scString m_workTarget;
};

#endif // _WORKQUEUECLIENTIMPL_H__