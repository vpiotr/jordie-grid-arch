/////////////////////////////////////////////////////////////////////////////
// Name:        Module.h
// Project:     grdLib
// Purpose:     Base class for processing modules
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMODULE_H__
#define _GRDMODULE_H__

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
#include "grd/Envelope.h"
#include "grd/Message.h"
#include "grd/Response.h"
#include "grd/Task.h"

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

class scModuleIntf: public scInterface 
{
public:
    scModuleIntf(){};
    virtual ~scModuleIntf(){};
    /// creates returns SC_MSG_STATUS_xxx
    virtual int handleMessage(const scEnvelope &envelope, scResponse &response) = 0;
    virtual int handleMessage(scMessage *message, scResponse &response) = 0;
    virtual int handleResponse(scMessage *message, scResponse *response) = 0;
    /// returns NULL if task cannot be created (unknown message)
    virtual scTaskIntf *prepareTaskForMessage(scMessage *message) = 0;
    virtual scTaskIntf *prepareTaskForResponse(scResponse *response) = 0;
    virtual scStringList supportedInterfaces() const = 0;
    virtual bool supportsInterface(const scString &name, const scString &version = scString("")) = 0;
};

#endif // _GRDMODULE_H__