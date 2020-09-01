/////////////////////////////////////////////////////////////////////////////
// Name:        Connection.h
// Project:     grdLib
// Purpose:     Base class for keeping connection status.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDCONNECT_H__
#define _GRDCONNECT_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Connection.h
\brief Base class for keeping connection status.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// sc
#include "sc/dtypes.h"

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

class scConnection {
public:
  // construction
  scConnection();
  virtual ~scConnection();
  // properties
  void setInactTimeout(uint msecs);
  // exec
  virtual void close() = 0;
  virtual bool isConnected() = 0;  
  void checkInactivity();
protected:  
  void signalConnected();
  void signalUsed();
  void performAutoClose();
protected:
  uint m_inactTimeout; // inactivity timeout for connections
  ulong64 m_lastUsedTime;
};

#endif // _GRDCONNECT_H__