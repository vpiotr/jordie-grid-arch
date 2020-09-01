/////////////////////////////////////////////////////////////////////////////
// Name:        RequestItem.h
// Project:     grdLib
// Purpose:     Request container for message observing.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDREQITEM_H__
#define _GRDREQITEM_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file RequestItem.h
\brief Request container for message observing.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// boost
#include "boost/ptr_container/ptr_map.hpp"

// grd
#include "grd/Envelope.h"
#include "grd/RequestHandler.h"

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
// scRequestItem
// ----------------------------------------------------------------------------
/// Information about request being processed
class scRequestItem 
{
public:
  scRequestItem(); 
  scRequestItem(const scEnvelope &envelope, scRequestHandlerTransporter &handlerTransporter);
  scRequestItem( const scRequestItem& rhs);            // copy constructor
  scRequestItem& operator=( const scRequestItem& rhs); // copy assignment operator
  virtual ~scRequestItem();
  scEnvelope *getEnvelope(); 
  scRequestHandler *getHandler();
  scRequestHandlerTransporter &getHandlerTransporter();
  cpu_ticks getStartTime() const; 
  uint getRequestId() const;
protected:
  void initStartTime();
protected:
  scEnvelope m_envelope;
  scRequestHandlerTransporter m_handlerTransporter;
  cpu_ticks m_startTime;
};
  
// ----------------------------------------------------------------------------
// scRequestItemMapColn
// ----------------------------------------------------------------------------
/// Collection of waiting to be processed envelopes (owning)
typedef boost::ptr_map<int,scRequestItem> scRequestItemMapColn; 
typedef boost::ptr_map<int,scRequestItem>::iterator scRequestItemMapColnIterator;

#endif // _GRDREQITEM_H__