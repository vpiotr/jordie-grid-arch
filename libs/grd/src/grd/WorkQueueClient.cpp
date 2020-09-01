/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueClient.cpp
// Project:     grdLib
// Purpose:     Client-side request execution support classes.
//              It is an interface between control program (client) and
//              worker server software.
// Author:      Piotr Likus
// Modified by:
// Created:     26/07/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/WorkQueueClient.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

//-------------------------------------------------------------------------------
// scWqRequest
//-------------------------------------------------------------------------------
bool scWqRequest::getHandlerName(scString &output) const
{
  output = "";
  return false;
}

bool scWqRequest::execute(const scString &command, const scDataNode &params, scDataNode &output)
{
  setCommand(command);
  setParams(params);
  return execute(output);
}

bool scWqRequest::execute(scDataNode &output)
{
  executeAsync();
  waitFor();
  return isResultOk();
}

void scWqRequest::executeAsync(const scString &command, const scDataNode &params)
{
  setCommand(command);
  setParams(params);
  executeAsync();
}

void scWqRequest::notify(const scString &command, const scDataNode &params)
{
  setCommand(command);
  setParams(params);
  notify();
}

//-------------------------------------------------------------------------------
// scWqRequestGroup
//-------------------------------------------------------------------------------
scWqRequestGroup::scWqRequestGroup()
{
}

scWqRequestGroup::~scWqRequestGroup()
{
}

// properties  
bool scWqRequestGroup::isResultReady()
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

bool scWqRequestGroup::isResultReady(uint index)
{  
  return getRequest(index).isResultReady();
}

bool scWqRequestGroup::isAnyResultReady()
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

bool scWqRequestGroup::isResultOk()
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

bool scWqRequestGroup::isResultOk(uint index)
{
  return getRequest(index).isResultOk();
}

uint scWqRequestGroup::size()
{
  return m_requests.size();
}

scWqRequest &scWqRequestGroup::getRequest(uint index)
{  
  return m_requests[index];
}

void scWqRequestGroup::removeRequest(uint index)
{
  if (!isResultReady(index))
    throw scError("Request not complited");
  m_requests.erase(m_requests.begin() + index);
}

void scWqRequestGroup::getResult(uint index, scDataNode &output)
{
  getRequest(index).getResult(output);
}

// run  
void scWqRequestGroup::mapRequest(const scString &command, const scDataNode &params)
{
  std::auto_ptr<scWqRequest> newRequestGuard;
  
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

void scWqRequestGroup::addRequest(scWqRequest *request)
{
  m_requests.push_back(request);
}

void scWqRequestGroup::checkStatus()
{
  for(uint i=0, epos = size(); i != epos; i++)
  {
    getRequest(i).checkStatus();
  }
}

bool scWqRequestGroup::execute(scDataNode &output)
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

void scWqRequestGroup::executeAsync()
{
  for(uint i=0, epos = size(); i != epos; i++)
  {
    getRequest(i).executeAsync();
  }
}

void scWqRequestGroup::notify()
{
  for(uint i=0, epos = size(); i != epos; i++)
  {
    getRequest(i).notify();
  }
}

