/////////////////////////////////////////////////////////////////////////////
// Name:        RequestHandler.cpp
// Project:     grdLib
// Purpose:     Request handler
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RequestHandler.h"


// ----------------------------------------------------------------------------
// scRequestHandler
// ----------------------------------------------------------------------------
scRequestHandler::scRequestHandler(): scReferenceCounter()
{
}

scRequestHandler::~scRequestHandler()
{
#ifdef SC_DEBUG_HANDLERS
  Log::addText("~scRequestHandler performed");
#endif  
}

void scRequestHandler::beforeTaskDelete(const scTaskIntf *task, bool &handlerForDelete)
{
  handlerForDelete = false;
}

void scRequestHandler::beforeReqQueued(const scEnvelope &a_envelope)
{ // do nothing
}

bool scRequestHandler::handleException(const scError &error)
{
  return false;
}
