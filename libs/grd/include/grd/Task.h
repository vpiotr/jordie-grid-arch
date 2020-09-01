/////////////////////////////////////////////////////////////////////////////
// Name:        Task.h
// Project:     grdLib
// Purpose:     Task object - processing entity.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _GRDTASK_H__
#define _GRDTASK_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Task.h
\brief Task object - processing entity.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"

#include "grd/Envelope.h"
#include "grd/Message.h"
#include "grd/Response.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
enum scTaskStatus { 
  tsCreated, 
  tsStarting, 
  tsRunning, 
  tsStopping, 
  tsStopped, 
  tsPaused, 
  tsDestroying,
  tsBusy
};

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scTaskIntf
{
public:
    scTaskIntf() {}
    virtual ~scTaskIntf() {}
    virtual int handleMessage(scEnvelope &envelope, scResponse &response) = 0;
    virtual int handleResponse(scMessage *message, scResponse &response) = 0;
    /// Returns true if message can be processed by task
    virtual bool acceptsMessage(const scString &command, const scDataNode &params) = 0;
    virtual scString getName() const = 0;
    virtual void setName(const scString &value) = 0;
    /// returns >1 if there is a need for more iterations
    virtual int run() = 0;
    virtual bool needsRun() = 0;
    virtual void requestStop() = 0;
    virtual bool isDaemon() = 0;
};

#endif // _GRDTASK_H__