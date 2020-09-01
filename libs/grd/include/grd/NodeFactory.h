/////////////////////////////////////////////////////////////////////////////
// Name:        NodeFactory.h
// Project:     grdLib
// Purpose:     Public interface to global processing node factory.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDNODEFACTRY_H__
#define _GRDNODEFACTRY_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file NodeFactory.h
\brief Public interface to global processing node factory.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
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
// scNodeFactory
// ----------------------------------------------------------------------------
/// Creates a new node basing on string class & name
class scNodeFactory {
public:
  scNodeFactory();
  virtual ~scNodeFactory();
  static scSchedulerIntf *createNode(const scString &a_className, const scString &nodeName);
  static bool factoryExists();
protected:
  virtual scSchedulerIntf *intCreateNode(const scString &a_className, const scString &nodeName) = 0;
  static scNodeFactory *checkFactory();
private:
  static scNodeFactory *m_activeFactory;
};


#endif // _GRDNODEFACTRY_H__