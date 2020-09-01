/////////////////////////////////////////////////////////////////////////////
// Name:        Envelope.cpp
// Project:     grdLib
// Purpose:     Full message contents + addresses + protocols...
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/Envelope.h"


// ----------------------------------------------------------------------------
// scEnvelope
// ----------------------------------------------------------------------------
scEnvelope::scEnvelope():m_event(SC_NULL) {
  m_timeout = 0;
}

scEnvelope::scEnvelope(scEnvelope const &rhs):m_event(SC_NULL) 
{
  //assign
  m_sender = rhs.m_sender;
  m_receiver = rhs.m_receiver;
  m_timeout = rhs.m_timeout;
  if (rhs.m_event != SC_NULL)
  {
    m_event = rhs.m_event->clone();
  }  
}

scEnvelope::scEnvelope(const scMessageAddress &sender, const scMessageAddress &receiver, scEvent *a_event):
m_sender(sender),m_receiver(receiver),m_event(a_event),m_timeout(0)
{  
}

scEnvelope& scEnvelope::operator=( const scEnvelope& rhs)
{
 if (&rhs != this) {
   //delete all resources of <this>
   delete m_event;
   m_event = SC_NULL;
   //assign
   m_sender = rhs.m_sender;
   m_receiver = rhs.m_receiver;
   m_timeout = rhs.m_timeout;
   if (rhs.m_event != SC_NULL)
   {
     m_event = rhs.m_event->clone();
   }  
 }
 return *this;
}

scEnvelope::~scEnvelope() 
{
  delete m_event;
}  

void scEnvelope::clear()
{
  setEvent(SC_NULL);
  m_sender.clear();
  m_receiver.clear();
  m_timeout = 0;
}


scEvent* scEnvelope::getEvent() const
{
  return m_event;
}

const scEvent &scEnvelope::getEventRef() const
{
  if (m_event != SC_NULL)
    return *m_event;
  else
    throw scError("Event not assigned!");  
}

scMessageAddress scEnvelope::getSender() const
{
  return m_sender;
}

scMessageAddress scEnvelope::getReceiver() const
{
  return m_receiver;
}

void scEnvelope::setSender(const scMessageAddress &address)
{
  m_sender = address;
}

void scEnvelope::setReceiver(const scMessageAddress &address)
{
  m_receiver = address;
}
  
void scEnvelope::setEvent(scEvent *a_event)
{
  delete m_event;
  m_event = a_event;
}  

void scEnvelope::setTimeout(uint a_timeout)
{
  m_timeout = a_timeout;
}

uint scEnvelope::getTimeout() const
{
  return m_timeout;
}
