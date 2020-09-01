/////////////////////////////////////////////////////////////////////////////
// Name:        SharedResourceModule.cpp
// Project:     scLib
// Purpose:     Module for handling messages for shared resources
// Author:      Piotr Likus
// Modified by:
// Created:     13/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "sc/SharedResource.h"

#include "grd/SharedResourceModule.h"

//#define SRM_DEBUG

scSharedResourceModule::scSharedResourceModule(): scModule()
{
}

scSharedResourceModule::~scSharedResourceModule()
{
}

int scSharedResourceModule::handleMessage(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_UNK_MSG;
  scString text;

  assert(message != SC_NULL);
  response.clearResult();

  if (message->getInterface() == "shres") 
  {
    if (message->getCoreCommand() == "release_ref") 
    {
      res = handleCmdReleaseRef(message, response);
    } 
  }        
  response.setStatus(res);
  return res;
}

scStringList scSharedResourceModule::supportedInterfaces() const
{
  scStringList res;
  res.push_back("shres");  
  return res;
}

int scSharedResourceModule::handleCmdReleaseRef(scMessage *message, scResponse &response)
{
  int res = SC_MSG_STATUS_WRONG_PARAMS;

  scDataNode &params = message->getParams(); 
  
  if (!params.empty())
  {
    scString resName = params.getString(0);
#ifdef SRM_DEBUG    
    scLog::addDebug("release_ref received on: ["+getScheduler()->getRegistrationId()+"], for: ["+resName+"]");             
#endif
    
    scSharedResourceManager::releaseRef(resName);
    res = SC_MSG_STATUS_OK;
  }     
  
  return res;
}

