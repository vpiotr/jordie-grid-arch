/////////////////////////////////////////////////////////////////////////////
// Name:        SharedResourceModule.h
// Project:     scLib
// Purpose:     Module for handling messages for shared resources
// Author:      Piotr Likus
// Modified by:
// Created:     13/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _SHAREDRESOURCEMODULE_H__
#define _SHAREDRESOURCEMODULE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file SharedResourceModule.h
///
/// Module which handles the following messages:
/// - shres.release_ref - release reference to shared memory block

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/core.h"

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
class scSharedResourceModule: public scModule 
{
public:
    scSharedResourceModule();
    virtual ~scSharedResourceModule();
    virtual int handleMessage(scMessage *message, scResponse &response);
    virtual scStringList supportedInterfaces() const;
protected:
    int handleCmdReleaseRef(scMessage *message, scResponse &response);
};


#endif // _SHAREDRESOURCEMODULE_H__