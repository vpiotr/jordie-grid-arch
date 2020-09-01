/////////////////////////////////////////////////////////////////////////////
// Name:        Event.cpp
// Project:     grdLib
// Purpose:     Base class for communication events
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/Event.h"


// ----------------------------------------------------------------------------
// scEvent
// ----------------------------------------------------------------------------
scEvent::scEvent() 
{
}

bool scEvent::isResponse() const
{
  return false;
}

int scEvent::getRequestId() const
{
  return m_requestId;
}  

void scEvent::setRequestId(int id)
{
  m_requestId = id;
}

scEvent *scEvent::clone() const
{
  scEvent *newEvent = new scEvent();
  newEvent->m_requestId = this->m_requestId;
  return newEvent;  
}

void scEvent::clear()
{
  m_requestId = SC_REQUEST_ID_NULL;
}


