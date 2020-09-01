/////////////////////////////////////////////////////////////////////////////
// Name:        LocalNodeRegistry.h
// Project:     grdLib
// Purpose:     In-memory message gate for communication between nodes the 
//              same address space.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDLOCNODREG_H__
#define _GRDLOCNODREG_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file FileName.h
\brief Short file description

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// boost
#include "boost/ptr_container/ptr_map.hpp"

// sc 
#include "sc\dtypes.h"

// grd
#include "grd/Scheduler.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scLocalNodeRegistry
// ----------------------------------------------------------------------------
/// List of in-memory node objects, access by name or iter, owning 
typedef boost::ptr_map<scString,scSchedulerIntf> scLocalNodeRegistryBase; 
typedef boost::ptr_map<scString,scSchedulerIntf>::iterator scLocalNodeRegistryIterator;

struct scTaskStatusKeeperBase;

class scLocalNodeRegistry: public scLocalNodeRegistryBase {
public: 
  scLocalNodeRegistry(): scLocalNodeRegistryBase() {};
  virtual ~scLocalNodeRegistry() {};
  void add(scSchedulerIntf *node);
  scSchedulerIntf *find(const scString &name);
};


#endif // _GRDLOCNODREG_H__