/////////////////////////////////////////////////////////////////////////////
// Name:        ResolveHandler.h
// Project:     grdLib
// Purpose:     Response handler for "core.advertise" message
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDRESOLVEHANDLR_H__
#define _GRDRESOLVEHANDLR_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file ResolveHandler.h
\brief Response handler for "core.advertise" message

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/RequestHandler.h"
#include "grd/Message.h"
#include "grd/Response.h"
#include "grd/details/SchedulerImpl.h"

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

class scResolveHandler: public scRequestHandler {
public: 
  // construction
  scResolveHandler();
  virtual ~scResolveHandler();  
  // properties
  void setScheduler(scScheduler *scheduler);
  void setOrgAddress(const scString &address);
  void setOrgCommand(const scString &command);
  void setOrgParams(const scDataNode *params);
  void setOrgRequestId(int requestId);
  void setOrgHandler(scRequestHandler *handler);
  // execution
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);    
protected:
  bool isSelectModeRandom();  
  void logResolveError(const scString &msg);
  void handleUnknownAlias();
protected:
  scScheduler *m_scheduler;  
  scString m_orgAddress;
  scString m_orgCommand;
  std::auto_ptr<scDataNode> m_orgParams;
  int m_orgRequestId;
  scRequestHandler *m_orgHandler;
};


#endif // _GRDRESOLVEHANDLR_H__