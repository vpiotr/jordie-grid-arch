/////////////////////////////////////////////////////////////////////////////
// Name:        RpcClient.h
// Project:     grdLib
// Purpose:     Client-side request execution support classes.
//              It is an interface between control program (client) and
//              worker server software.
// Author:      Piotr Likus
// Modified by:
// Created:     26/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDRPCCLIENT_H__
#define _GRDRPCCLIENT_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
/// Usage examples:
/// 1) 
///  myreq = server.newRequest();
///  if (!myreq->execute("init_island", islandParams, reqResult)) 
///     myreq->throwLastError();
///
/// 2)
///  mygrp1 = server.newGroup();
///  mygrp1->mapRequest("eval_island", islandParams);  
///
///  mygrp1->waitForAll();
///  mygrp1->checkStatus();
///  
///  ... consume mygrp1->getResult(0..k)

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// boost
#include "boost/ptr_container/ptr_list.hpp"
// sc
#include "sc/dtypes.h"

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
class scRpcRequest;
class scRpcRequestGroup;
class scRpcServerProxy;

typedef boost::shared_ptr<scRpcServerProxy> scRpcServerProxyTransporter;
typedef std::map<scString,scRpcServerProxyTransporter> scRpcServerProxyRegistryMap;  

// factory of requests
class scRpcServerProxy {
public:
  scRpcServerProxy() {}   
  virtual ~scRpcServerProxy() {};   
  virtual scRpcRequest *newRequest() = 0;
  virtual scRpcRequestGroup *newGroup() = 0;
  static scRpcServerProxyTransporter getProxy(const scString &serviceName = "");
  virtual bool execute(const scString &command, const scDataNode &params, scDataNode &output);
  virtual void notify(const scString &command, const scDataNode &params);
protected:
  static scRpcServerProxyRegistryMap m_proxyMap;
};

class scRpcRequestMonitor {
public: 
  scRpcRequestMonitor() {}
  virtual ~scRpcRequestMonitor() {}

  virtual void handleGroupHandled(scRpcRequestGroup &group) {}  
};

class scRpcRequest {
public:
  scRpcRequest() {}
  virtual ~scRpcRequest() {};
// properties
  virtual bool isResultReady() const = 0;
  virtual bool isResultOk() const = 0;
  virtual bool getResult(scDataNode &output) const = 0;
  virtual bool getStatus(scDataNode &output) const = 0;
  virtual void setCommand(const scString &command) = 0;
  virtual const scString &getCommand() const = 0;  
  virtual void setParams(const scDataNode &params) = 0;
  virtual bool getParams(scDataNode &output) const = 0;  
  virtual bool getHandlerName(scString &output) const;
// run
  virtual bool execute(const scString &command, const scDataNode &params, scDataNode &output);
  virtual void executeAsync(const scString &command, const scDataNode &params);
  virtual bool execute(scDataNode &output);
  virtual void executeAsync() = 0;
  virtual void notify(const scString &command, const scDataNode &params);
  virtual void notify() = 0;
  virtual void cancel() = 0;
  virtual void waitFor() = 0;
  virtual void throwLastError() = 0;
  virtual void checkStatus() = 0;
};

typedef boost::ptr_vector<scRpcRequest> scRpcRequestColn;

class scRpcRequestGroup {
public:
// create
  scRpcRequestGroup();
  virtual ~scRpcRequestGroup();
// properties  
  virtual bool isResultReady();
  virtual bool isResultReady(uint index);
  virtual bool isAnyResultReady();
  virtual bool isResultOk();
  virtual bool isResultOk(uint index);
  virtual uint size();
  virtual scRpcRequest &getRequest(uint index);
  virtual void getResult(uint index, scDataNode &output);
  virtual void removeRequest(uint index);
// run  
  virtual void mapRequest(const scString &command, const scDataNode &params);
  virtual void addRequest(scRpcRequest *request);
  virtual bool execute(scDataNode &output);
  virtual void executeAsync();
  virtual void notify();
  virtual void waitFor() = 0;  
  virtual uint waitForAny() = 0; /// returns first ready request index that was not rdy before call
  virtual void checkStatus();
protected:  
  virtual scRpcRequest *newRequest() = 0;
protected:  
  scRpcRequestColn m_requests;
};

#endif // _GRDRPCCLIENT_H__