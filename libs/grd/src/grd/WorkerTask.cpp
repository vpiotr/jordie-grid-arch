/////////////////////////////////////////////////////////////////////////////
// Name:        WorkerTask.cpp
// Project:     scLib
// Purpose:     Task which handles asynchronymously a portion of work and
//              returns result. No persistency (restart) support.
// Author:      Piotr Likus
// Modified by:
// Created:     05/05/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////
#include "grd/WorkerTask.h"

scWorkerTask::scWorkerTask(const scEnvelope &requestEnvelope, scScheduler *scheduler): scTask() 
{
  m_scheduler = scheduler;
  m_reqEnvelope = requestEnvelope;
}

scWorkerTask::~scWorkerTask() 
{
}

scScheduler *scWorkerTask::getScheduler() 
{
  return m_scheduler;
}

const scDataNode &scWorkerTask::getWorkParams() const
{
  scMessage *message = dynamic_cast<scMessage *> (m_reqEnvelope.getEvent());
  return message->getParams(); 
}

bool scWorkerTask::needsRun()
{
  return true;
}

void scWorkerTask::postResult(const scDataNode &resultData)
{
   scResponse response;
   response.setResult(resultData);
   response.setStatus(SC_MSG_STATUS_OK);

   postResponse(response);
}

void scWorkerTask::postError(int code)
{
  int useCode = (code == 0)?SC_MSG_STATUS_ERROR:code;

   scResponse response;
   response.setStatus(useCode);

   postResponse(response);
}

void scWorkerTask::postError(int code, const scDataNode &errorDetails)
{
  int useCode = (code == 0)?SC_MSG_STATUS_ERROR:code;

   scResponse response;
   response.setError(errorDetails);
   response.setStatus(useCode);

   postResponse(response);
}

void scWorkerTask::postError(int code, const scString &details)
{
  int useCode = (code == 0)?SC_MSG_STATUS_ERROR:code;

   scResponse response;
   scDataNode detailsNode;
   detailsNode.setElementSafe("text", details);
   
   response.setError(detailsNode);
   response.setStatus(useCode);

   postResponse(response);
}

void scWorkerTask::postResponse(const scResponse &response)
{   
   std::auto_ptr<scEnvelope> envelopeGuard(
     new scEnvelope(
       scMessageAddress(m_reqEnvelope.getReceiver()), 
       scMessageAddress(m_reqEnvelope.getSender()), 
       new scResponse(response)));
       
   scEnvelope *outEnvelope = envelopeGuard.get();
   //copy requestId from original message
   outEnvelope->getEvent()->setRequestId(m_reqEnvelope.getEvent()->getRequestId());
   //post response to original sender
   getScheduler()->postEnvelope(envelopeGuard.release());  
}
