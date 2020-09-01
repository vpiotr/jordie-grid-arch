/////////////////////////////////////////////////////////////////////////////
// Name:        Response.cpp
// Project:     grdLib
// Purpose:     Response event container.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/Response.h"
#include "grd/MessageConst.h"

// ----------------------------------------------------------------------------
// scResponse
// ----------------------------------------------------------------------------
scResponse::scResponse()
{
}

scResponse::scResponse(scResponse const &rhs)
{
  if (this != &rhs)
  {
    this->m_status = rhs.m_status;
    this->m_requestId = rhs.m_requestId;
    this->m_error = rhs.m_error;
    this->m_result = rhs.m_result;
  }  
}

scResponse::~scResponse()
{
#ifdef SC_LOG_CORE
#ifdef SC_DEBUG
Log::addText("~scResponse");
#endif
#endif
}

scResponse& scResponse::operator=( const scResponse& rhs)
{
  if (this != &rhs)
  {
    this->m_status = rhs.m_status;
    this->m_requestId = rhs.m_requestId;
    this->m_error = rhs.m_error;
    this->m_result = rhs.m_result;
  }  
  return *this;
}

void scResponse::clearResult() 
{
  setErrorNull();
  setResultNull();
}
  
scEvent *scResponse::clone() const
{
  scResponse *newResponse = new scResponse(*this);
  return newResponse;  
}

void scResponse::setStatus(int a_status) 
{ 
  m_status = a_status;
}
  
void scResponse::setResultNull() 
{
  m_result.clear();
}

void scResponse::setResult(const scDataNode &a_result)
{
  m_result = a_result;
}

void scResponse::setResult(base::move_ptr<scDataNode> a_result)
{
  m_result = a_result;
}

void scResponse::setErrorNull() 
{
  m_error.clear();
}

void scResponse::setError(const scDataNode &a_error)
{
  m_error = a_error;
  if (m_status >= 0)
    m_status = SC_RESP_STATUS_UNDEF_ERROR;
}

int scResponse::getStatus() const 
{
  return m_status;
}
  
scDataNode &scResponse::getResult() const
{
  return const_cast<scDataNode &>(m_result);
}
  
scDataNode &scResponse::getError() const
{
  return const_cast<scDataNode &>(m_error);
}

bool scResponse::isResponse() const
{
  return true;
}

bool scResponse::isError() const
{
  return (m_status < 0);
} 

void scResponse::clear()
{
  scEvent::clear();
  m_status = SC_RESP_STATUS_UNDEF_ERROR;
  m_result.clear();
  m_error.clear();
}

void scResponse::initFor(const scMessage &src)
{
  setRequestId(src.getRequestId());
}
