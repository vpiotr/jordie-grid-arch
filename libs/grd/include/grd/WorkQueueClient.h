/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueClient.h
// Project:     grdLib
// Purpose:     Client-side request execution support classes.
//              It is an interface between control program (client) and
//              worker server software.
// Author:      Piotr Likus
// Modified by:
// Created:     26/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _WORKQUEUECLIENT_H__
#define _WORKQUEUECLIENT_H__

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
class scWqRequest;
class scWqRequestGroup;

// factory of requests
class scWqServerProxy {
public:
  scWqServerProxy() {}   
  virtual ~scWqServerProxy() {};   
  virtual scWqRequest *newRequest() = 0;
  virtual scWqRequestGroup *newGroup() = 0;
};

class scWqRequestMonitor: public scObject {
public: 
  scWqRequestMonitor() {}
  virtual ~scWqRequestMonitor() {}

  virtual void handleGroupHandled(scWqRequestGroup &group) {}  
};

class scWqRequest {
public:
  scWqRequest() {}
  virtual ~scWqRequest() {};
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

typedef boost::ptr_vector<scWqRequest> scWqRequestColn;

class scWqRequestGroup {
public:
// create
  scWqRequestGroup();
  virtual ~scWqRequestGroup();
// properties  
  virtual bool isResultReady();
  virtual bool isResultReady(uint index);
  virtual bool isAnyResultReady();
  virtual bool isResultOk();
  virtual bool isResultOk(uint index);
  virtual uint size();
  virtual scWqRequest &getRequest(uint index);
  virtual void getResult(uint index, scDataNode &output);
  virtual void removeRequest(uint index);
// run  
  virtual void mapRequest(const scString &command, const scDataNode &params);
  virtual void addRequest(scWqRequest *request);
  virtual bool execute(scDataNode &output);
  virtual void executeAsync();
  virtual void notify();
  virtual void waitFor() = 0;  
  virtual uint waitForAny() = 0; // returns first ready request index that was not rdy before call
  virtual void checkStatus();
protected:  
  virtual scWqRequest *newRequest() = 0;
protected:  
  scWqRequestColn m_requests;
};

#endif // _WORKQUEUECLIENT_H__
