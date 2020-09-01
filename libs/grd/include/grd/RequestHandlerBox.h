/////////////////////////////////////////////////////////////////////////////
// Name:        RequestHandlerBox.h
// Project:     grdLib
// Purpose:     Request handler with response buffer.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDREQHANDLRBOX_H__
#define _GRDREQHANDLRBOX_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file RequestHandlerBox.h
\brief Short file description

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/RequestHandler.h"
#include "grd/Message.h"
#include "grd/Response.h"

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
// scRequestHandlerBox
// ----------------------------------------------------------------------------  
/// Handler that holds (1) result for requester 
class scRequestHandlerBox: public scRequestHandler 
{
public:
  scRequestHandlerBox();
  virtual ~scRequestHandlerBox();
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
  virtual bool handleException(const scError &error); 
  virtual bool isResponseReady() const;
  scResponse getResponse() const;
  virtual void clear(); 
protected:
  scMessage m_message;
  scResponse m_response;
  bool m_responseReady;
};


#endif // _GRDREQHANDLRBOX_H__