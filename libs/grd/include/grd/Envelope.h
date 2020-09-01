/////////////////////////////////////////////////////////////////////////////
// Name:        Envelope.h
// Project:     grdLib
// Purpose:     Full message contents + addresses + protocols...
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDENVELOPE_H__
#define _GRDENVELOPE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Envelope.h
\brief Full message contents + addresses + protocols...

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// boost
#include "boost/ptr_container/ptr_list.hpp"

// base
#include "base/serializer.h"

// sc
#include "sc\dtypes.h"

// grd
#include "grd\MessageAddress.h"
#include "grd\Event.h"

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
// scEnvelope
// ----------------------------------------------------------------------------
/// Full message contents + addresses + protocols...
class scEnvelope
{
public:
    scEnvelope();
    scEnvelope(scEnvelope const &rhs);    
    scEnvelope(const scMessageAddress &sender, const scMessageAddress &receiver, scEvent *a_event);
    scEnvelope& operator=( const scEnvelope& rhs);
    virtual ~scEnvelope();  
    scEvent* getEvent() const;
    const scEvent &getEventRef() const;
    scMessageAddress getSender() const;
    scMessageAddress getReceiver() const;
    void setSender(const scMessageAddress &address);
    void setReceiver(const scMessageAddress &address);
    void setEvent(scEvent *a_event);
    void setTimeout(uint a_timeout);
    uint getTimeout() const;
    void clear();
protected:
    scEvent* m_event;
    scMessageAddress m_sender;
    scMessageAddress m_receiver;
    uint m_timeout; ///< in ms
};

// ----------------------------------------------------------------------------
// scEnvelopeColn
// ----------------------------------------------------------------------------
/// Collection of envelopes (owning)
typedef boost::ptr_list<scEnvelope> scEnvelopeColn;
typedef boost::ptr_list<scEnvelope>::iterator scEnvelopeColnIterator;
typedef scEnvelopeColn::auto_type scEnvelopeTransport;

// ----------------------------------------------------------------------------
// Other
// ----------------------------------------------------------------------------
typedef dtp::dtpSerializerIntf<scEnvelope> scEnvelopeSerializerBase;

#endif // _GRDENVELOPE_H__