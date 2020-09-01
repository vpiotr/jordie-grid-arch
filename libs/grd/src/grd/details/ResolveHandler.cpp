/////////////////////////////////////////////////////////////////////////////
// Name:        ResolveHandler.cpp
// Project:     grdLib
// Purpose:     Response handler for "core.advertise" message
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/details/ResolveHandler.h"
#include "base/rand.h"
#include "perf/time_utils.h"
#include "perf/Log.h"

using namespace perf;

// ----------------------------------------------------------------------------
// scResolveHandler
// ----------------------------------------------------------------------------
scResolveHandler::scResolveHandler(): scRequestHandler()
{
}

scResolveHandler::~scResolveHandler()
{
  if (m_orgHandler != SC_NULL)
    delete m_orgHandler;
}

void scResolveHandler::setScheduler(scScheduler *scheduler)
{
  m_scheduler = scheduler;
}

void scResolveHandler::setOrgAddress(const scString &address)
{
  m_orgAddress = address;
}

void scResolveHandler::setOrgCommand(const scString &command)
{
  m_orgCommand = command;
}

void scResolveHandler::setOrgParams(const scDataNode *params)
{
  m_orgParams.reset(new scDataNode(*params));
}

void scResolveHandler::setOrgRequestId(int requestId)
{
  m_orgRequestId = requestId;
}

void scResolveHandler::setOrgHandler(scRequestHandler *handler)
{
  m_orgHandler = handler;
}

void scResolveHandler::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  scString foundAddr;
  if (!a_response.getResult().empty()) {
    uint addressIdx;
    if (a_response.getResult().size() == 1)
      addressIdx = 0;
    else {
      if (isSelectModeRandom())
        addressIdx = randomUInt(0, a_response.getResult().size() - 1);
      else
        addressIdx = 0;  
    }  
    if (a_response.getResult().size() > addressIdx)
    {
      scDataNode address;
      a_response.getResult().getElement(addressIdx, address);
      if (address.hasChild("address"))
        foundAddr = address.getString("address");
      else  
        foundAddr = address.getAsString();

      cpu_ticks timeToLive = address.getUInt64("share_time", 0);
      if (timeToLive > 0) // if there is a time limit
        timeToLive += cpu_time_ms();
                
      scString tempName;         
      if (!m_scheduler->hasNodeInRegistry(m_orgAddress)) 
        m_scheduler->registerNodeAs(m_orgAddress, foundAddr, tempName, false, true, 0, timeToLive);  
      m_scheduler->postMessage(foundAddr, m_orgCommand, m_orgParams.get(), m_orgRequestId, m_orgHandler);
      m_orgHandler = SC_NULL;
    }
  } else {
  // empty address list - try to forward
    handleUnknownAlias();
  }
}

void scResolveHandler::handleReqError(const scMessage &a_message, const scResponse &a_response)  
{
  logResolveError(scString("Addr resolve error: ")+a_response.getError().getString("text", ""));  
  if (m_orgHandler != SC_NULL)
    m_orgHandler->handleReqError(a_message, a_response);
}

void scResolveHandler::handleUnknownAlias()
{ 
  //scSchedulerImpl *schedulerImpl = dynamic_cast<scSchedulerImpl *>(m_scheduler);
  //if (schedulerImpl != SC_NULL) 
  //  schedulerImpl->handleResolveFailed(m_orgAddress, m_orgCommand, m_orgParams.get(), m_orgRequestId, m_orgHandler);
  if (!m_scheduler->forwardMessage(m_orgAddress, m_orgCommand, m_orgParams.get(), m_orgRequestId, m_orgHandler))
  {
    Log::addError(scString("Unknown receiver") + " [" + m_orgAddress + "]");
    if (m_orgHandler != SC_NULL)
    {
      scMessage dummyMessage;
      scResponse response;
      response.setError(scDataNode(scString("Error - unknown receiver: [") + m_orgAddress + "]"));
      m_orgHandler->handleReqError(dummyMessage, response);
    }  
  }  
}

void scResolveHandler::logResolveError(const scString &msg)
{
  Log::addError(msg + " [" + m_orgAddress + "]");
}

bool scResolveHandler::isSelectModeRandom()
{
  return true;
}
