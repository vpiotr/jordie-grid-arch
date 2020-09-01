/////////////////////////////////////////////////////////////////////////////
// Name:        BoostMsgQueueGate.h
// Project:     grdLib
// Purpose:     Boost-based message gate.
// Author:      Piotr Likus
// Modified by:
// Created:     17/04/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _BOOSTMSGQUEUEGATE_H__
#define _BOOSTMSGQUEUEGATE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file BoostMsgQueueGate.h
///
/// File description

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
class grdBmqGateFactory: public scGateFactory {
public:
  grdBmqGateFactory();
  virtual ~grdBmqGateFactory();
  virtual scMessageGate *createGate(bool input, const scDataNode &params, const scString &protocol) const;
protected:
};


#endif // _BOOSTMSGQUEUEGATE_H__