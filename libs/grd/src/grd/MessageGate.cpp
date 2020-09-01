/////////////////////////////////////////////////////////////////////////////
// Name:        MessageGate.cpp
// Project:     grdLib
// Purpose:     Base class for message gates
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/MessageGate.h"
#include "grd/MessageConst.h"

#ifdef SC_LOG_ERRORS
#include "perf/Log.h"
using namespace perf;
#endif

#include "grd/MessageTrace.h"

// ----------------------------------------------------------------------------
// scMessageGate
// ----------------------------------------------------------------------------
scMessageGate::scMessageGate()
{
}

scMessageGate::~scMessageGate() 
{
}

void scMessageGate::put(scEnvelope* envelope)
{
  m_waiting.push_front(envelope);
}

scEnvelope* scMessageGate::get()
{
  if (!m_waiting.empty()) {
    scEnvelopeTransport transp = m_waiting.pop_back();
    //m_waiting.release( m_waiting.begin());
    return transp.release();
  }  
  else
    throw scError("Message gate is empty");     
}

bool scMessageGate::empty()
{
  return m_waiting.empty();
}

void scMessageGate::init()
{//empty  
}

scEnvelope *scMessageGate::createErrorResponseFor(const scEnvelope &srcEnvelope, const scString &msg, int a_status)
{
  scEnvelope* renvelope;
  scSchedulerIntf *node;  

  node = getOwner(); 
  assert(node != SC_NULL);

  renvelope = node->createErrorResponseFor(srcEnvelope, msg, a_status);
    
  return renvelope;    
}

void scMessageGate::handleTransmitError(const scEnvelope &envelope, const scError &e)
{
  int error;

  if (e.getErrorCode() != 0)
    error = e.getErrorCode();
  else
    error = SC_RESP_STATUS_TRANSMIT_ERROR;
    
  scString errorMsg =   
    "Unknown transmit error["+toString(error)+"]: "+e.what();

  handleTransmitError(envelope, error, errorMsg, e.getDetails());    
}

void scMessageGate::handleTransmitError(const scEnvelope &envelope, int errorCode, const scString &errorMsg, const scString &details)
{
  scEnvelope* renvelope;
  scSchedulerIntf *node = getOwner();
  assert(node != SC_NULL);
  int error;

  if (errorCode != 0)
    error = errorCode;
  else
    error = SC_RESP_STATUS_TRANSMIT_ERROR;
    
  renvelope = createErrorResponseFor(envelope, errorMsg, error);      
    
  node->postEnvelopeForThis(renvelope);

#ifdef SC_LOG_ERRORS
  Log::addError(error, errorMsg);
#endif  
}

scSchedulerIntf *scMessageGate::getOwner() 
{
  return m_owner;
}

void scMessageGate::setOwner(scSchedulerIntf *a_owner)
{
  m_owner = a_owner;
}

bool scMessageGate::getOwnAddress(const scString &protocol, scMessageAddress &output)
{
  return false;
}

scString scMessageGate::getOwnerName()
{
  return m_owner->getName();
}

void scMessageGate::handleMsgReceived(const scEnvelope &envelope)
{    
#ifdef TRACE_MSGS
  scString eventCode;

  if (envelope.getEvent()->isResponse())
    eventCode = "gate_resp_recv";
  else
    eventCode = "gate_req_recv";

  addMsgTrace(eventCode, envelope);
#endif
}

void scMessageGate::handleMsgReadyForSend(const scEnvelope &envelope)
{
#ifdef TRACE_MSGS
  scString eventCode;

  if (envelope.getEvent()->isResponse())
    eventCode = "gate_resp_rdy";
  else
    eventCode = "gate_req_rdy";

  addMsgTrace(eventCode, envelope);
#endif
}

void scMessageGate::handleMsgSent(const scEnvelope &envelope)
{
#ifdef TRACE_MSGS
  scString eventCode;

  if (envelope.getEvent()->isResponse())
    eventCode = "gate_resp_sent";
  else
    eventCode = "gate_req_sent";

  addMsgTrace(eventCode, envelope);
#endif
}

void scMessageGate::addMsgTrace(const scString &eventCode, const scEnvelope &envelope)
{
  int requestId = SC_REQUEST_ID_NULL;
  scString command;

  if (!envelope.getEvent()->isResponse())
  {
    scMessage &message = *dynamic_cast<scMessage *>(envelope.getEvent());
    command = message.getCommand();
  }

  requestId = envelope.getEvent()->getRequestId();

  scMessageTrace::addTrace(
      eventCode, 
      envelope.getSender().getAsString(), 
      envelope.getReceiver().getAsString(), 
      requestId, 
      command
  );
}

