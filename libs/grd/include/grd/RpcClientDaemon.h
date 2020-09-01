/////////////////////////////////////////////////////////////////////////////
// Name:        RpcClientDaemon.h
// Project:     grdLib
// Purpose:     Daemon for execution of RPC client interface.
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDRPCCLIDEM_H__
#define _GRDRPCCLIDEM_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file RpcClientDaemon.h
\brief Daemon for execution of RPC client interface.

It can be used in parallel with other grdLib-based software as additional
processing. 

It allows construction of RPC server proxies through factories.
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// sc
#include "sc/Events.h"
// grd
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
class scRpcClientDaemon {
public:
    // construct
    scRpcClientDaemon();
    virtual ~scRpcClientDaemon();
    // attributes
    virtual void setScheduler(scScheduler *value);
    virtual void setYieldSignal(scSignal *value);
    // run
    virtual void init();
    virtual bool runStep();
    virtual bool needsRun();
protected:
    void checkPrepared();
    virtual scObject *newBody();
    void prepareBody();
protected:
    std::auto_ptr<scObject> m_body;
};

#endif // _GRDRPCCLIDEM_H__