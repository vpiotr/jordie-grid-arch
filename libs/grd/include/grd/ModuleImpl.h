/////////////////////////////////////////////////////////////////////////////
// Name:        Module.h
// Project:     grdLib
// Purpose:     Base class for processing modules
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMODULEIMPL_H__
#define _GRDMODULEIMPL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Module.h
\brief Short file description

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/Message.h"
#include "grd/Response.h"
#include "grd/Envelope.h"
#include "grd/Scheduler.h"
#include "grd/Task.h"
#include "grd/Module.h"

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
// scModule
// ----------------------------------------------------------------------------
/// Factory selecting tasks basing on provided request
class scModule: public scModuleIntf
{
public:
    scModule(){};
    virtual ~scModule(){};
    /// creates returns SC_MSG_STATUS_xxx
    virtual int handleMessage(const scEnvelope &envelope, scResponse &response);
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual int handleResponse(scMessage *message, scResponse *response);
    /// returns NULL if task cannot be created (unknown message)
    virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
    virtual scTaskIntf *prepareTaskForResponse(scResponse *response);
    virtual scStringList supportedInterfaces() const = 0;
    virtual bool supportsInterface(const scString &name, const scString &version = scString(""));
    // -- properties --
    void setScheduler(const scSchedulerIntf *scheduler);
    scSchedulerIntf *getScheduler();
protected:
    scSchedulerIntf *m_scheduler;    
};


#endif // _GRDMODULEIMPL_H__