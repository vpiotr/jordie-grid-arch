/////////////////////////////////////////////////////////////////////////////
// Name:        Connection.h
// Project:     grdLib
// Purpose:     Base class for keeping connection status.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/Connection.h"
#include "perf/time_utils.h"

//----------------------------------------------------------------------------------
// scConnection
//----------------------------------------------------------------------------------
scConnection::scConnection(): m_lastUsedTime(0), m_inactTimeout(0)
{
}

scConnection::~scConnection()
{
}

void scConnection::setInactTimeout(uint msecs)
{
  this->m_inactTimeout = msecs;
}

void scConnection::checkInactivity()
{
  if (m_inactTimeout > 0)
    if ((m_lastUsedTime > 0) && is_cpu_time_elapsed_ms(m_lastUsedTime, m_inactTimeout))
      performAutoClose();
}

void scConnection::performAutoClose()
{
  if (isConnected())
    close();
}

void scConnection::signalUsed()
{
  m_lastUsedTime = cpu_time_ms();
}

void scConnection::signalConnected()
{
  signalUsed();
}
