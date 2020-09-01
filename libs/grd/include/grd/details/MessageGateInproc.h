/////////////////////////////////////////////////////////////////////////////
// Name:        MessageGateInproc.h
// Project:     grdLib
// Purpose:     In-memory message gate for communication between nodes the 
//              same address space.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _MSGGATEINPR_H__
#define _MSGGATEINPR_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file MessageGateInproc.h
\brief In-memory message gate for communication between nodes

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/MessageGate.h"
#include "grd/LocalNodeRegistry.h"
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
// supported protocol names
const scString SC_PROTOCOL_INPROC  = "inproc";  ///< inproc

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scMessageGateInproc
// ----------------------------------------------------------------------------
/// In-proc gate version of gate
class scMessageGateInproc: public scMessageGate {
public:
    scMessageGateInproc();
    virtual ~scMessageGateInproc();
    virtual bool supportsProtocol(const scString &protocol);
    void setLocalRegistry(scLocalNodeRegistry *registry);
    // --- properties ---
    void setOwnerName(const scString &a_name);
    scString getOwnerName() const; 
    virtual scSchedulerIntf *getOwner();    
    virtual bool getOwnAddress(const scString &protocol, scMessageAddress &output);
protected:
    scSchedulerIntf *getLocalNodeByName(const scString &a_name);
protected:    
    scLocalNodeRegistry *m_localRegistry;
    scString m_ownerName;
};

// ----------------------------------------------------------------------------
// scMessageGateInprocIn
// ----------------------------------------------------------------------------
/// Input-only inproc gate
class scMessageGateInprocIn: public scMessageGateInproc {
public:
    scMessageGateInprocIn();
    virtual ~scMessageGateInprocIn();
    virtual int run();
};

// ----------------------------------------------------------------------------
// scMessageGateInprocOut
// ----------------------------------------------------------------------------
/// Output-only inproc gate
class scMessageGateInprocOut: public scMessageGateInproc {
public:
    scMessageGateInprocOut();
    virtual ~scMessageGateInprocOut();
    virtual int run();
protected:    
    virtual void handleUnknownReceiver(const scEnvelope &envelope);
};


#endif // _MSGGATEINPR_H__