/////////////////////////////////////////////////////////////////////////////
// Name:        ConnectionPool.h
// Project:     grdLib
// Purpose:     Collection of active connections
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDCONNPOOL_H__
#define _GRDCONNPOOL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file ConnectionPool.h
\brief Collection of active connections

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// boost
#include "boost/ptr_container/ptr_map.hpp"

// grd
#include "grd/Connection.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
typedef boost::ptr_map<scString,scConnection> scConnectionMap;

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

class scConnectionPool {
public:
  void add(const scString &connectionId, scConnection *item);
  scConnection *find(const scString &connectionId);
  void checkActive();
  template<typename T>
  void forEach(T functor) {
    for(scConnectionMap::iterator it = m_connections.begin(), epos = m_connections.end(); it != epos; ++it)
    {
      functor(it->second);
    }
  }
protected:
  scConnectionMap m_connections;    
};



#endif // _GRDCONNPOOL_H__