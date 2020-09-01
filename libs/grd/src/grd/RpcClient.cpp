/////////////////////////////////////////////////////////////////////////////
// Name:        RpcClient.cpp
// Project:     grdLib
// Purpose:     Client-side request execution support classes.
//              It is an interface between control program (client) and
//              worker server software.
// Author:      Piotr Likus
// Modified by:
// Created:     26/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RpcClient.h"
#include "grd/RpcServerProxyFactory.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

//-------------------------------------------------------------------------------
// scRpcServerProxy
//-------------------------------------------------------------------------------
scRpcServerProxyRegistryMap scRpcServerProxy::m_proxyMap;

scRpcServerProxyTransporter scRpcServerProxy::getProxy(const scString &serviceName)
{
    scRpcServerProxyTransporter res;

    scRpcServerProxyRegistryMap::const_iterator it = m_proxyMap.find(serviceName);

    if (it == m_proxyMap.end())
    {
        res.reset(scRpcServerProxyFactory::newServerProxy(serviceName));
        m_proxyMap.insert(std::make_pair(serviceName, res));
    } else {
        res = it->second;
    }

    return res;
}

bool scRpcServerProxy::execute(const scString &command, const scDataNode &params, scDataNode &output)
{
    std::auto_ptr<scRpcRequest> request(newRequest());
    //Note: we do not return error details so we need to raise exception on error here
    if (!request->execute(command, params, output))
        request->throwLastError();
    return true;
}

void scRpcServerProxy::notify(const scString &command, const scDataNode &params)
{
    std::auto_ptr<scRpcRequest> request(newRequest());
    request->notify(command, params);
}

//-------------------------------------------------------------------------------
// scRpcRequest
//-------------------------------------------------------------------------------
bool scRpcRequest::getHandlerName(scString &output) const
{
  output = "";
  return false;
}

bool scRpcRequest::execute(const scString &command, const scDataNode &params, scDataNode &output)
{
  setCommand(command);
  setParams(params);
  return execute(output);
}

bool scRpcRequest::execute(scDataNode &output)
{
  executeAsync();
  waitFor();
  return isResultOk();
}

void scRpcRequest::executeAsync(const scString &command, const scDataNode &params)
{
  setCommand(command);
  setParams(params);
  executeAsync();
}

void scRpcRequest::notify(const scString &command, const scDataNode &params)
{
  setCommand(command);
  setParams(params);
  notify();
}

//-------------------------------------------------------------------------------
// scRpcRequestGroup
//-------------------------------------------------------------------------------
scRpcRequestGroup::scRpcRequestGroup()
{
}

scRpcRequestGroup::~scRpcRequestGroup()
{
}

// properties  
bool scRpcRequestGroup::isResultReady()
{
  bool res = true;
  for(uint i=0, epos = size(); i != epos; i++)
  {
    if (!isResultReady(i))
    {
      res = false;
      break;
    }  
  }
  return res;  
}

bool scRpcRequestGroup::isResultReady(uint index)
{  
  return getRequest(index).isResultReady();
}

bool scRpcRequestGroup::isAnyResultReady()
{
  bool res = false;
  for(uint i=0, epos = size(); i != epos; i++)
  {
    if (isResultReady(i))
    {
      res = true;
      break;
    }  
  }
  return res;  
}

bool scRpcRequestGroup::isResultOk()
{
  bool res = true;
  for(uint i=0, epos = size(); i != epos; i++)
  {
    if (!isResultOk(i))
    {
      res = false;
      break;
    }  
  }
  return res;  
}

bool scRpcRequestGroup::isResultOk(uint index)
{
  return getRequest(index).isResultOk();
}

uint scRpcRequestGroup::size()
{
  return m_requests.size();
}

scRpcRequest &scRpcRequestGroup::getRequest(uint index)
{  
  return m_requests[index];
}

void scRpcRequestGroup::removeRequest(uint index)
{
  if (!isResultReady(index))
    throw scError("Request not complited");
  m_requests.erase(m_requests.begin() + index);
}

void scRpcRequestGroup::getResult(uint index, scDataNode &output)
{
  getRequest(index).getResult(output);
}

// run  
void scRpcRequestGroup::mapRequest(const scString &command, const scDataNode &params)
{
  std::auto_ptr<scRpcRequest> newRequestGuard;
  
  if (params.size() > 1) {
    for(uint i=0, epos = params.size(); i != epos; i++)
    {
      newRequestGuard.reset(newRequest());
      newRequestGuard->setCommand(command);
      newRequestGuard->setParams(params.getElement(i));
      addRequest(newRequestGuard.release());
    }
  } else {
    newRequestGuard.reset(newRequest());
    newRequestGuard->setCommand(command);
    newRequestGuard->setParams(params);
    addRequest(newRequestGuard.release());
  }
}

void scRpcRequestGroup::addRequest(scRpcRequest *request)
{
  m_requests.push_back(request);
}

void scRpcRequestGroup::checkStatus()
{
  for(uint i=0, epos = size(); i != epos; i++)
  {
    getRequest(i).checkStatus();
  }
}

bool scRpcRequestGroup::execute(scDataNode &output)
{
  bool res = true;
  for(uint i=0, epos = size(); i != epos; i++)
  {
    if (!getRequest(i).execute(output))
    {
      res = false;
    }  
  }
  return res;  
}

void scRpcRequestGroup::executeAsync()
{
  for(uint i=0, epos = size(); i != epos; i++)
  {
    getRequest(i).executeAsync();
  }
}

void scRpcRequestGroup::notify()
{
  for(uint i=0, epos = size(); i != epos; i++)
  {
    getRequest(i).notify();
  }
}

