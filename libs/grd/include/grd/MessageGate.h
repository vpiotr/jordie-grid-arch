/////////////////////////////////////////////////////////////////////////////
// Name:        MessageGate.h
// Project:     grdLib
// Purpose:     Base class for message gates
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMSGGATE_H__
#define _GRDMSGGATE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file MessageGate.h
\brief Base class for message gates

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/Envelope.h"
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
// scMessageGate
// ----------------------------------------------------------------------------
/// Communication channel
class scMessageGate
{
public:
    scMessageGate();
    virtual ~scMessageGate();
    // -- envelope handling
    void put(scEnvelope* envelope);
    scEnvelope* get();
    bool empty();
    // -- major functions
    virtual bool supportsProtocol(const scString &protocol) = 0;
    virtual bool getOwnAddress(const scString &protocol, scMessageAddress &output);
    // transmit messages
    virtual int run() = 0;
    virtual void init(); 
    scSchedulerIntf *getOwner();    
    void setOwner(scSchedulerIntf *a_owner);
protected:
    scEnvelope *createErrorResponseFor(const scEnvelope &srcEnvelope, const scString &msg, int a_status);
    void handleTransmitError(const scEnvelope &envelope, const scError &e);
    void handleTransmitError(const scEnvelope &envelope, int errorCode, const scString &errorMsg, const scString &details = "");
    scString getOwnerName();
    virtual void handleMsgReceived(const scEnvelope &envelope);
    virtual void handleMsgReadyForSend(const scEnvelope &envelope);
    virtual void handleMsgSent(const scEnvelope &envelope);
    void addMsgTrace(const scString &eventCode, const scEnvelope &envelope);
private:
    scEnvelopeColn m_waiting;           
    scSchedulerIntf *m_owner; 
};

#endif // _GRDMSGGATE_H__