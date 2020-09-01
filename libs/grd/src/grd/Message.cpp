/////////////////////////////////////////////////////////////////////////////
// Name:        Message.cpp
// Project:     grdLib
// Purpose:     Message container for a request sent to processor
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/Message.h"


// ----------------------------------------------------------------------------
// scMessage
// ----------------------------------------------------------------------------
scMessage::scMessage()
{
  m_requestId = SC_REQUEST_ID_NULL;
}

scMessage::scMessage(const scString &command, 
      const scDataNode *a_params, 
      const int requestId)
{
  m_command = command;
  m_requestId = requestId;
  if (a_params != SC_NULL)
    m_params = *a_params;    
}

scEvent *scMessage::clone() const
{
  scMessage *newMessage = new scMessage(*this);
  return newMessage;  
}

scMessage::scMessage(scMessage const &rhs)
{
  copyFrom(rhs);
}

scMessage& scMessage::operator=( const scMessage& rhs)
{
  if (this != &rhs)
    copyFrom(rhs);  
  return *this;  
}

void scMessage::copyFrom(const scMessage& rhs)
{
  m_command = rhs.getCommand();
  m_requestId = rhs.getRequestId();
  m_params = (const_cast< scMessage &>(rhs)).getParams();
}

scString scMessage::getCommand() const
{ 
  return m_command; 
}

scString scMessage::getCoreCommand() const
{  
  scString res;
  size_t startpos = m_command.find(".");
  if (scString::npos != startpos)
     res = m_command.substr( startpos + 1 ); 
  else 
     res = m_command;   
  return res;   
}

scString scMessage::getInterface() const
{
  scString res;
  size_t startpos = m_command.find(".");
  if (scString::npos != startpos)
     res = m_command.substr( 0, startpos ); 
  return res;   
}

scDataNode &scMessage::getParams()
{
  return m_params;
}  

void scMessage::setCommand(const scString &a_command)
{
  m_command = a_command;
}

void scMessage::setParams(const scDataNode &a_params)
{
  m_params = a_params;
}

bool scMessage::hasParams() const
{
    return !m_params.empty();
}

bool scMessage::hasRequestId() const
{
    return (m_requestId != SC_REQUEST_ID_NULL);
}

void scMessage::clear()
{
  scEvent::clear();
  m_command = "";
  m_params.clear();
}
