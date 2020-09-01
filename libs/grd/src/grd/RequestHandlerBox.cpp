/////////////////////////////////////////////////////////////////////////////
// Name:        RequestHandlerBox.cpp
// Project:     grdLib
// Purpose:     Request handler with response buffer.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/RequestHandlerBox.h"
#include "grd/MessageConst.h"

// ----------------------------------------------------------------------------
// scRequestHandlerBox
// ----------------------------------------------------------------------------
scRequestHandlerBox::scRequestHandlerBox(): 
  scRequestHandler(),
  m_responseReady(false),
  m_response(),
  m_message()
{
}
scRequestHandlerBox::~scRequestHandlerBox()
{
}

void scRequestHandlerBox::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  m_message = a_message;
  m_response = a_response;
  m_responseReady = true;
}

void scRequestHandlerBox::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  m_message = a_message;
  m_response = a_response;
  m_responseReady = true;
}

bool scRequestHandlerBox::handleException(const scError &error)
{
  scDataNode *params = new scDataNode();
  boost::shared_ptr<scDataNode> guard(params);
  scString msg(error.what());
  params->addChild("text", new scDataNode(msg));

  m_response.clear();
  m_response.setStatus(SC_MSG_STATUS_EXCEPTION);
  m_response.setError(*params);
  m_responseReady = true;
  
  return true;
}

bool scRequestHandlerBox::isResponseReady() const
{
  return m_responseReady;
}

scResponse scRequestHandlerBox::getResponse() const
{
  return m_response;
}  

void scRequestHandlerBox::clear()
{
  m_responseReady = false;
  m_response.clear();
}



