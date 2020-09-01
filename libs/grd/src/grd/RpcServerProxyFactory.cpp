/////////////////////////////////////////////////////////////////////////////
// Name:        RpcServerProxyFactory.cpp
// Project:     grdLib
// Purpose:     Factory class of RPC server proxies.
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RpcServerProxyFactory.h"

scRpcServerProxyFactory *scRpcServerProxyFactory::m_activeObject = SC_NULL;

scRpcServerProxyFactory::scRpcServerProxyFactory()
{
    if (m_activeObject == SC_NULL)
        m_activeObject = this;
}

scRpcServerProxyFactory::~scRpcServerProxyFactory()
{
    if (m_activeObject == this)
        m_activeObject = SC_NULL;
}

scRpcServerProxy *scRpcServerProxyFactory::newServerProxy(const scString &serviceName)
{
    if (m_activeObject == SC_NULL)
        throw scError("Rpc server proxy factory not ready!");
    return m_activeObject->newServerProxyThis(serviceName);
}

