/////////////////////////////////////////////////////////////////////////////
// Name:        LocalNodeRegistry.cpp
// Project:     grdLib
// Purpose:     In-memory message gate for communication between nodes the 
//              same address space.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/LocalNodeRegistry.h"


// ----------------------------------------------------------------------------
// scLocalNodeRegistry
// ----------------------------------------------------------------------------
void scLocalNodeRegistry::add(scSchedulerIntf *node)
{
  scString name = node->getName();
  insert(name, node);
}

scSchedulerIntf *scLocalNodeRegistry::find(const scString &name)
{
  scLocalNodeRegistryIterator p;

  p = scLocalNodeRegistryBase::find(name);
  if(p != end())
    return const_cast<scSchedulerIntf *>(&(*(p->second)));
  else
    return SC_NULL;  
}

