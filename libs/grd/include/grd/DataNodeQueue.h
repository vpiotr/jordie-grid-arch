/////////////////////////////////////////////////////////////////////////////
// Name:        DataNodeQueue.h
// Project:     grdLib
// Purpose:     Synchronized storage queue for dnode objects.
// Author:      Piotr Likus
// Modified by:
// Created:     28/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _GRDDNODEQUEUE_H__
#define _GRDDNODEQUEUE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file DataNodeQueue.h
\brief Synchronized storage queue for dnode objects.

*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include <boost/thread.hpp>

#include "sc\dtypes.h"

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

class grdDataNodeQueue {
public:
  grdDataNodeQueue();
  ~grdDataNodeQueue();
  void put(const scDataNode &message);
  bool tryGet(scDataNode &output);
  bool peek(scDataNode &output);
  bool empty();
  size_t size();
  void eraseTop();
private:
  boost::mutex m_mutex;
  scDataNode m_data;
};


#endif // _GRDDNODEQUEUE_H__