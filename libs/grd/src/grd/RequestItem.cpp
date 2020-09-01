/////////////////////////////////////////////////////////////////////////////
// Name:        RequestItem.cpp
// Project:     grdLib
// Purpose:     Request container for message observing.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RequestItem.h"
#include "perf/time_utils.h"

// ----------------------------------------------------------------------------
// scRequestItem
// ----------------------------------------------------------------------------
scRequestItem::scRequestItem():m_envelope(), m_handlerTransporter()
{
  initStartTime();
}

scRequestItem::scRequestItem(const scEnvelope &envelope, scRequestHandlerTransporter &handlerTransporter): 
  m_envelope(envelope), m_handlerTransporter(handlerTransporter)
{  
  initStartTime();
}

scRequestItem::scRequestItem( const scRequestItem& rhs)
{
  m_envelope = *(const_cast<scRequestItem&>(rhs).getEnvelope());
  m_handlerTransporter = const_cast<scRequestItem&>(rhs).getHandlerTransporter();
  m_startTime = rhs.m_startTime;
}


scRequestItem& scRequestItem::operator=( const scRequestItem& rhs)
{
  if (this != &rhs)
  {
    m_envelope = *(const_cast<scRequestItem&>(rhs).getEnvelope());
    m_handlerTransporter = const_cast<scRequestItem&>(rhs).getHandlerTransporter();
    m_startTime = rhs.m_startTime;
  }
  return *this;  
}

scRequestItem::~scRequestItem()
{
}

scEnvelope *scRequestItem::getEnvelope()
{
  return &m_envelope;
}

scRequestHandler *scRequestItem::getHandler()
{
  return m_handlerTransporter.get();
}

scRequestHandlerTransporter &scRequestItem::getHandlerTransporter()
{
  return m_handlerTransporter;
}

void scRequestItem::initStartTime()
{
  m_startTime = cpu_time_ms();
}

cpu_ticks scRequestItem::getStartTime() const
{
  return m_startTime;
}

uint scRequestItem::getRequestId() const
{
  return m_envelope.getEvent()->getRequestId();
}
