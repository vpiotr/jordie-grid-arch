/////////////////////////////////////////////////////////////////////////////
// Name:        ConnectionPool.cpp
// Project:     grdLib
// Purpose:     Collection of active connections
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/ConnectionPool.h"


//----------------------------------------------------------------------------------
// scConnectionPool
//----------------------------------------------------------------------------------
scConnection *scConnectionPool::find(const scString &connectionId)
{
  scConnectionMap::iterator p;

  p = m_connections.find(connectionId);
  if(p != m_connections.end())
    return p->second;
  else
    return SC_NULL;  
}

void scConnectionPool::add(const scString &connectionId, scConnection *item)
{
  m_connections.insert(const_cast<scString &>(connectionId), item);
}

void scConnectionPool::checkActive()
{
  scConnectionMap::iterator it;
  for(it = m_connections.begin(); it != m_connections.end(); ++it)
    it->second->checkInactivity();

  it = m_connections.begin();
  while(it != m_connections.end())
  {
    if (!((*it).second->isConnected()))
      it = m_connections.erase(it);
    else
      ++it;
  }    
}

