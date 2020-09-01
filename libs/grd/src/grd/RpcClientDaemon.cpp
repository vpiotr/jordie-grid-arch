/////////////////////////////////////////////////////////////////////////////
// Name:        RpcClientDaemon.cpp
// Project:     grdLib
// Purpose:     Daemon for execution of RPC client interface.
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RpcClientDaemon.h"
#include "grd/RpcClientImpl.h"
#include "grd/RpcServerProxyFactory.h"

// ----------------------------------------------------------------------------
// Private declarations
// ----------------------------------------------------------------------------

/// Implementation body of daemon, can be published in separate header file 
/// when new implementation will be needed.
class scRpcClientDaemonBody: public scObject {
public:
    // construct
    scRpcClientDaemonBody();
    virtual ~scRpcClientDaemonBody();
    // attributes
    virtual void setScheduler(scScheduler *value);
    virtual void setYieldSignal(scSignal *value);
    // run 
    virtual void init();
    virtual bool runStep();
    virtual bool needsRun();
protected:
    void checkPrepared();
    void prepareProxyFactory();
protected:
    bool m_prepared;
    scScheduler *m_scheduler;
    scSignal *m_yieldSignal;
    std::auto_ptr<scRpcServerProxyFactory> m_proxyFactory;
}; 

class scRpcServerProxyFactoryForDaemon: public scRpcServerProxyFactory {
public:
    scRpcServerProxyFactoryForDaemon(scScheduler *scheduler, scSignal *yieldSignal): 
      scRpcServerProxyFactory(), 
      m_scheduler(scheduler),
      m_yieldSignal(yieldSignal) 
      {}
    virtual ~scRpcServerProxyFactoryForDaemon() {}
    virtual scRpcServerProxy *newServerProxyThis(const scString &serviceName) {
        return new scRpcServerProxyImpl(m_scheduler, m_yieldSignal, getServiceAddress(serviceName));
    }
    virtual scString getServiceAddress(const scString &serviceName) {
        if ((!serviceName.empty()) && (!scMessageAddress::isRoleName(serviceName)))
            return scMessageAddress::buildRoleName(serviceName);
        else
            return serviceName;
    }
protected:
    scScheduler *m_scheduler;
    scSignal *m_yieldSignal;
};

// ----------------------------------------------------------------------------
// Private implementations
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scRpcClientDaemonBody
// ----------------------------------------------------------------------------
scRpcClientDaemonBody::scRpcClientDaemonBody(): scObject(), m_yieldSignal(SC_NULL), m_scheduler(SC_NULL), m_prepared(false)
{
}

scRpcClientDaemonBody::~scRpcClientDaemonBody()
{
}

void scRpcClientDaemonBody::setScheduler(scScheduler *value)
{
    m_scheduler = value;
}

void scRpcClientDaemonBody::setYieldSignal(scSignal *value)
{
    m_yieldSignal = value;
}

void scRpcClientDaemonBody::checkPrepared()
{
    if (!m_prepared)
        throw scError("RPC client daemon not ready!");
}

void scRpcClientDaemonBody::init()
{
    if (m_prepared)
        return;

    if (
          (m_scheduler != SC_NULL)
          &&
          (m_yieldSignal != SC_NULL)
       )
    {
        prepareProxyFactory();
        m_prepared = true;
    } else {
      // throw error if something is not prepared 
      checkPrepared();
    }
}

bool scRpcClientDaemonBody::runStep()
{
   checkPrepared();
   m_scheduler->run();
   return needsRun();
}

bool scRpcClientDaemonBody::needsRun()
{
   checkPrepared();
   return m_scheduler->needsRun();
}

void scRpcClientDaemonBody::prepareProxyFactory()
{
    m_proxyFactory.reset(new scRpcServerProxyFactoryForDaemon(m_scheduler, m_yieldSignal));
}

// ----------------------------------------------------------------------------
// Public implementations
// ----------------------------------------------------------------------------

scRpcClientDaemon::scRpcClientDaemon()
{
}

scRpcClientDaemon::~scRpcClientDaemon()
{
}

// attributes
void scRpcClientDaemon::setScheduler(scScheduler *value)
{
    prepareBody();
    dynamic_cast<scRpcClientDaemonBody *>(m_body.get())->setScheduler(value);
}

void scRpcClientDaemon::setYieldSignal(scSignal *value)
{
    prepareBody();
    dynamic_cast<scRpcClientDaemonBody *>(m_body.get())->setYieldSignal(value);
}

// run
void scRpcClientDaemon::init()
{
    prepareBody();
    dynamic_cast<scRpcClientDaemonBody *>(m_body.get())->init();
}

bool scRpcClientDaemon::runStep()
{
    checkPrepared();
    return dynamic_cast<scRpcClientDaemonBody *>(m_body.get())->runStep();
}

bool scRpcClientDaemon::needsRun()
{
    checkPrepared();
    return dynamic_cast<scRpcClientDaemonBody *>(m_body.get())->needsRun();
}

void scRpcClientDaemon::checkPrepared()
{
    if (m_body.get() == SC_NULL)
        throw scError("Rpc daemon body not prepared!");
}

scObject *scRpcClientDaemon::newBody()
{
    return new scRpcClientDaemonBody();
}

void scRpcClientDaemon::prepareBody()
{
    if (m_body.get() == SC_NULL)
        m_body.reset(newBody());
}

