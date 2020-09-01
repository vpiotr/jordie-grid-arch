/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesQueue.h
// Project:     grdLib
// Purpose:     Synchronized message queue for named pipes gates.
// Author:      Piotr Likus
// Modified by:
// Created:     06/10/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _W32NPQUEUE_H__
#define _W32NPQUEUE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file W32NamedPipesQueue.h
\brief Synchronized message queue for named pipes gates.

*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "boost/ptr_container/ptr_list.hpp"
#include <boost/thread.hpp>

#include "grd\W32NamedPipesGatesRaw.h"

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

/// Synchronized message queue for named pipes
class grdW32NamedPipesQueue: public grdW32NamedPipesQueueIntf {
  typedef boost::ptr_list<grdW32NamedPipesMessageIntf> grdW32NamedPipesMessageList;
public:
  grdW32NamedPipesQueue();
  virtual ~grdW32NamedPipesQueue();
  virtual size_t size() const;
  virtual bool empty() const;
  virtual void put(const grdW32NamedPipesMessageIntf &message);
  virtual bool tryGet(grdW32NamedPipesMessageIntf &message);
  virtual bool peek(grdW32NamedPipesMessageIntf &message);
  virtual void eraseTop();
private:
  grdW32NamedPipesMessageList m_messages;
  mutable boost::mutex m_mutex;
};


#endif // _W32NPQUEUE_H__