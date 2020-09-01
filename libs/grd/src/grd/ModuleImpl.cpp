/////////////////////////////////////////////////////////////////////////////
// Name:        Module.cpp
// Project:     grdLib
// Purpose:     Base class for processing modules
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/ModuleImpl.h"
#include "grd/MessageConst.h"

// ----------------------------------------------------------------------------
// scModule
// ----------------------------------------------------------------------------
scTaskIntf *scModule::prepareTaskForMessage(scMessage *message) 
{
  return SC_NULL;
}

scTaskIntf *scModule::prepareTaskForResponse(scResponse *response)
{
  return SC_NULL;
}  

bool scModule::supportsInterface(const scString &name, const scString &version)
{
  scStringList list = supportedInterfaces();
  scStringList::iterator p;
  
  p = find(list.begin(), // get an iterator into the vector that refers to an
           list.end(),   // element whose value is 100. If there is no such element
           name);        // then j will be equal to int_vector.end()
           
  if (p != list.end())
    return true;
  else
    return false;           
}

int scModule::handleMessage(const scEnvelope &envelope, scResponse &response)
{
  scMessage *message = dynamic_cast<scMessage *> (envelope.getEvent());
  return handleMessage(message, response);
}

int scModule::handleMessage(scMessage *message, scResponse &response)
{
  return SC_MSG_STATUS_UNK_MSG;
}

int scModule::handleResponse(scMessage *message, scResponse *response)
{
  return SC_MSG_STATUS_UNK_MSG;
}

void scModule::setScheduler(const scSchedulerIntf *scheduler)
{
  m_scheduler = const_cast<scSchedulerIntf *>(scheduler);
}

scSchedulerIntf *scModule::getScheduler()
{
  return m_scheduler;
}

