/////////////////////////////////////////////////////////////////////////////
// Name:        RpcClientImpl.h
// Project:     grdLib
// Purpose:     Implementation of work queue client interface.
// Author:      Piotr Likus
// Modified by:
// Created:     28/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDRPCCLIIMPL_H__
#define _GRDRPCCLIIMPL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file RpcClientImpl.h
///
/// File description

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/Events.h"
#include "grd/core.h"
#include "grd/RpcClient.h"

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
class scRpcServerProxyImpl: public scRpcServerProxy {
public:
  scRpcServerProxyImpl(scScheduler *scheduler, scSignal *waitSignal, const scString &workTarget);
  virtual ~scRpcServerProxyImpl() {}
  virtual scRpcRequest *newRequest();
  virtual scRpcRequestGroup *newGroup();
protected:
  scScheduler *m_scheduler; 
  scSignal *m_waitSignal; 
  scString m_workTarget;
};

#endif // _GRDRPCCLIIMPL_H__