/////////////////////////////////////////////////////////////////////////////
// Name:        RequestHandler.h
// Project:     grdLib
// Purpose:     Request handler
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDREQHANDLER_H__
#define _GRDREQHANDLER_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file RequestHandler.h
\brief Request handler

No more details.
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------

// boost
#include <boost/intrusive_ptr.hpp>

// sc
#include "sc/dtypes.h"

// grd
#include "grd/Task.h"
#include "grd/Message.h"
#include "grd/Response.h"
#include "grd/Envelope.h"

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
// scRequestHandler
// ----------------------------------------------------------------------------  
/// Handles outcome of request
class scRequestHandler: public scReferenceCounter
{ 
public:
  enum RequestPhase {rpPost, rpSend, rpReceive};
  scRequestHandler();
  virtual ~scRequestHandler();
  virtual void beforeTaskDelete(const scTaskIntf *task, bool &handlerForDelete);
  /// called before request is queued
  virtual void beforeReqQueued(const scEnvelope &a_envelope);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response) = 0;
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response) = 0;
  virtual bool handleException(const scError &error); 
protected:
};

typedef boost::intrusive_ptr<scRequestHandler> scRequestHandlerTransporter;

#endif // _GRDREQHANDLER_H__