/////////////////////////////////////////////////////////////////////////////
// Name:        RpcServerProxyFactory.h
// Project:     grdLib
// Purpose:     Factory class of RPC server proxies.
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDRPCSRVPROXFACT_H__
#define _GRDRPCSRVPROXFACT_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file RpcServerProxyFactory.h
\brief Factory class of RPC server proxies.

Use to automatically define server proxy object basing on service name.
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
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
const scString RPC_DEFAULT_SERVICE = "default";

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scRpcServerProxyFactory {
public:
    scRpcServerProxyFactory();
    virtual ~scRpcServerProxyFactory();
    static scRpcServerProxy *newServerProxy(const scString &serviceName);
    virtual scRpcServerProxy *newServerProxyThis(const scString &serviceName) = 0;
protected:
    static scRpcServerProxyFactory *m_activeObject;
};

#endif // _GRDRPCSRVPROXFACT_H__