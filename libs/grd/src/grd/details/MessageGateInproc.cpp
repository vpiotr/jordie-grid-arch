/////////////////////////////////////////////////////////////////////////////
// Name:        MessageGateInproc.cpp
// Project:     grdLib
// Purpose:     In-memory message gate for communication between nodes the 
//              same address space.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/details/MessageGateInproc.h"
#include "grd/MessageConst.h"

// ----------------------------------------------------------------------------
// scMessageGateInproc
// ----------------------------------------------------------------------------
scMessageGateInproc::scMessageGateInproc(): scMessageGate() 
{
}

scMessageGateInproc::~scMessageGateInproc() 
{
}

bool scMessageGateInproc::supportsProtocol(const scString &protocol)
{
   if ((protocol == SC_PROTOCOL_INPROC) || (protocol.empty()))
     return true;
   else
     return false;  
}
    
void scMessageGateInproc::setLocalRegistry(scLocalNodeRegistry *registry)
{
  m_localRegistry = registry;
}

void scMessageGateInproc::setOwnerName(const scString &a_name)
{
  m_ownerName = a_name;
}

scString scMessageGateInproc::getOwnerName() const
{
  return m_ownerName;
}  

scSchedulerIntf *scMessageGateInproc::getOwner() 
{
  scSchedulerIntf *node = getLocalNodeByName(getOwnerName());  
  return node;
}

scSchedulerIntf *scMessageGateInproc::getLocalNodeByName(const scString &a_name)
{
  scSchedulerIntf *res;
  assert(m_localRegistry != SC_NULL);
  
  res = m_localRegistry->find(a_name);
  
  return res;
} 

bool scMessageGateInproc::getOwnAddress(const scString &protocol, scMessageAddress &output)
{
  bool res = false;
  if ((protocol == SC_PROTOCOL_INPROC) || protocol.empty())
  {
    output.clear();
    output.setProtocol(protocol);
    output.setNode(getOwnerName());    
    res = true;
  }
  return res;
}

// ----------------------------------------------------------------------------
// scMessageGateInprocIn
// ----------------------------------------------------------------------------
scMessageGateInprocIn::scMessageGateInprocIn(): scMessageGateInproc() 
{
}

scMessageGateInprocIn::~scMessageGateInprocIn() 
{
}

int scMessageGateInprocIn::run()
{
  // for input gate - do nothing
  return 0;
}

// ----------------------------------------------------------------------------
// scMessageGateInprocOut
// ----------------------------------------------------------------------------
scMessageGateInprocOut::scMessageGateInprocOut(): scMessageGateInproc() 
{
}

scMessageGateInprocOut::~scMessageGateInprocOut() 
{
}

// for outgoing gates - move message from source.out_queue to dest.input_queue
int scMessageGateInprocOut::run()
{
  int res = 0;
  scString nodeName;
  scMessageAddress target;
  scSchedulerIntf *node;  
  std::auto_ptr<scEnvelope> envelopeGuard;
  
  // input
  while(!empty()) 
  {
    envelopeGuard.reset(get());
    handleMsgReceived(*envelopeGuard);
    res++;
    target = envelopeGuard->getReceiver();
    node = getLocalNodeByName(target.getNode());
    if (node != SC_NULL) 
    {
      node->postEnvelopeForThis(envelopeGuard.release());
    } else {
      handleUnknownReceiver(*envelopeGuard);
    }
  } // while   
  
  return res;
}

void scMessageGateInprocOut::handleUnknownReceiver(const scEnvelope &envelope)
{
  scEnvelope* renvelope;
  scSchedulerIntf *node;  

  if (!envelope.getEvent()->isResponse())
  {
    node = getLocalNodeByName(getOwnerName());  
    assert(node != SC_NULL);

    renvelope = createErrorResponseFor(envelope, 
      "Unknown node: ["+envelope.getReceiver().getNode()+"]", 
      SC_RESP_STATUS_UNKNOWN_NODE);      
    
    node->postEnvelopeForThis(renvelope);
  } else {
    throw scError("Unknown receiver for response: ["+(envelope.getReceiver().getAsString())+"]");
  }  
}
