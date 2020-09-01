/////////////////////////////////////////////////////////////////////////////
// Name:        NamedPipesGateIntf.h
// Project:     Named pipes gates
// Purpose:     Gates for named pipes -  (w/o framework) version.
// Author:      Piotr Likus
// Modified by:
// Created:     28/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _NAMPIPGTINT_H__
#define _NAMPIPGTINT_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file NamedPipesGateIntf.h
\brief Gates for named pipes -  (w/o framework) version.

*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include <string>

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
class NamedPipesGateInIntf {
public:
  virtual bool hasMessage() = 0;
  virtual void getMessage(std::string &outMsg) = 0;
};

class NamedPipesGateOutIntf {
public:
  virtual void putMessage(const std::string &aMsg) = 0;
  virtual unsigned int run() = 0;
};

#endif // _NAMPIPGTINT_H__