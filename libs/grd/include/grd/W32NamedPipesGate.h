/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesGate.h
// Project:     grdLib
// Purpose:     Win32 named pipes gate.
// Author:      Piotr Likus
// Modified by:
// Created:     29/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDW32NAMPIPMSGQUEUEGATE_H__
#define _GRDW32NAMPIPMSGQUEUEGATE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file W32NamedPipesGate.h
///
/// Win32 named pipes gate.

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/core.h"
#include "grd/GateFactory.h"

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
class grdW32NamedPipesGateFactory: public scGateFactory {
public:
  grdW32NamedPipesGateFactory();
  virtual ~grdW32NamedPipesGateFactory();
  virtual scMessageGate *createGate(bool input, const scDataNode &params, const scString &protocol) const;
protected:
};


#endif // _GRDW32NAMPIPMSGQUEUEGATE_H__