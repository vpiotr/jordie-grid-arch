/////////////////////////////////////////////////////////////////////////////
// Name:        MessageAddress.cpp
// Project:     grdLib
// Purpose:     Message address container
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/MessageAddress.h"


// ----------------------------------------------------------------------------
// scMessageAddress
// ----------------------------------------------------------------------------

scMessageAddress::scMessageAddress()
{
  clear();
  m_format = AdrFmtDefault;
}

scMessageAddress::scMessageAddress( const scString& src)
{
  clear();
  setAsString(src);  
}

scMessageAddress::scMessageAddress( const scMessageAddress& rhs) 
{
  setAsString(rhs.getAsString());
}

scMessageAddress& scMessageAddress::operator=( const scMessageAddress& rhs)    
{
  if (this != &rhs) 
  {
    setAsString(rhs.getAsString());
  }
  return *this;
}

scMessageAddress::~scMessageAddress()
{
}

void scMessageAddress::set(const scString &address)
{
  setAsString(address);
}

scString scMessageAddress::getProtocol() const
{
  return m_protocol;
}

scString scMessageAddress::getHost() const
{
  return m_host;
}
scString scMessageAddress::getNode() const
{
  return m_node;
}
scString scMessageAddress::getTask() const
{
  return m_task;
}
scString scMessageAddress::getRole() const
{
  return m_role;
}

scString scMessageAddress::getPath() const
{
  return m_vpath;
}

void scMessageAddress::setProtocol(const scString &a_protocol)
{
  m_protocol = a_protocol;
}

void scMessageAddress::setTask(const scString &a_value)
{
  m_task = a_value;
}

void scMessageAddress::setHost(const scString &a_value)
{
  m_host = a_value;
}

void scMessageAddress::setNode(const scString &a_value)
{
  m_node = a_value;
}

scMessageAddress::scAddressFormat scMessageAddress::getFormat() const
{
  return m_format;
}  

void scMessageAddress::setAsString(const scString &value)
{
  parseAddress(value);
}

scString scMessageAddress::getAsString() const
{
  return buildAddress();
}

bool scMessageAddress::isEmpty()
{
  return ((m_format == AdrFmtDefault) && (getAsString() == "#"));
}

void scMessageAddress::parseAddress(const scString &address)
{
  scString stack, protocol, host, node, task, role, vpath;
  int i, c, cp1;
  int ctx;
  bool fixed_fmt = false;
  enum {CTX_PROTOCOL, CTX_ADDR, CTX_ROLE_NAME, 
    CTX_FIXED_HOST, CTX_FIXED_NODE, CTX_FIXED_TASK,
    CTX_VPATH, CTX_END};
  
  ctx = CTX_PROTOCOL;
  i = 0;
  while( i<(int)address.length() )  
  {
    c = address[i];

    if (i<(int)address.length()-1) cp1 = address[i+1];
    else cp1 = 0;
        
    if (isCtl(c) || !isAscii(c)) {     
      throwAddressError(address, i, c); 
    }  
    if (isSpecial(c)) {
        switch (c)
        {
        case '@': 
          if ((ctx == CTX_FIXED_TASK) && task.empty()) {
              task += char(c);
              ++i;
          } else if (ctx != CTX_PROTOCOL && ctx != CTX_ADDR) {
            throwAddressError(address, i, c); 
          } else {
            ctx = CTX_ROLE_NAME;
            ++i;
          }
          break;
        case '#': 
          if (ctx != CTX_PROTOCOL && ctx != CTX_ADDR)
            throwAddressError(address, i, c); 
          ctx = CTX_FIXED_HOST;
          fixed_fmt = true;
          ++i;
          break;
        case '/': 
//---------------------------------        
        switch (ctx)
        {
        case CTX_PROTOCOL: 
        case CTX_ADDR: 
          if (cp1 != '/')
            throwAddressError(address, i, c); 
          ctx = CTX_VPATH;  
          vpath = "//";
          i+=2;
          break;
        case CTX_VPATH:
          vpath += char(c);
          ++i;
          break;
        case CTX_FIXED_HOST:
          ctx = CTX_FIXED_NODE;
          ++i;
          break;
        case CTX_FIXED_NODE:
          ctx = CTX_FIXED_TASK;
          ++i;
          break;
        case CTX_FIXED_TASK:
          ctx = CTX_END;
          ++i;
          break;
        default:
          throwAddressError(address, i, c); 
          break;
        } // switch context   
        break; // special char case
//---------------------------------          
        case ':':
          if (ctx != CTX_PROTOCOL || cp1 != ':')
            throwAddressError(address, i, c); 
          i+=2;
          ctx = CTX_ADDR;            
          break;
        default:
          throwAddressError(address, i, c); 
          break;
        } // switch    
    } else { // not special
        if (c == ' ') ++i;
        else
        switch (ctx)
        {
        case CTX_PROTOCOL: 
          protocol += char(c);
          ++i;
          break;
        case CTX_ADDR: 
          ctx = CTX_FIXED_HOST;
          break;
        case CTX_ROLE_NAME: 
          role += char(c);
          ++i;
          break;
        case CTX_FIXED_HOST:
          host += char(c);
          ++i;
          break;
        case CTX_FIXED_NODE:
          node += char(c);
          ++i;
          break;
        case CTX_FIXED_TASK:
          task += char(c);
          ++i;
          break;
        case CTX_VPATH:
          vpath += char(c);
          ++i;
          break;
        default:
          throwAddressError(address, i, c); 
          break;
        } // switch context   
    } // else !special
  } // while
  if (!role.empty())
  {
    clear();
    m_format = AdrFmtRole;
    m_protocol = protocol;
    m_role = role;
  }    
  else if (!vpath.empty())
  {
    clear();
    m_format = AdrFmtVPath;
    m_protocol = protocol;
    m_vpath = vpath;
  }  
  else 
  {
    clear();
    if (protocol == address)
    {
      m_format = AdrFmtRaw;    
      m_rawAddress = address;
    } else {      
      m_format = AdrFmtDefault;    
      m_protocol = protocol;
      m_host = host;
      m_node = node;
      m_task = task;
    }
  }  
}

void scMessageAddress::clear() {
    m_protocol = "";
    m_host = "";
    m_node = "";
    m_task = "";  
    m_vpath = "";  
    m_role = ""; 
    m_format = AdrFmtDefault; 
}
   
void scMessageAddress::throwAddressError(const scString &address, int a_pos, int a_char) const {
   throw scError("Wrong character ["+toString(a_char)+"] in address ["+address+"] at pos: "+toString(a_pos));
}

void scMessageAddress::throwUnknownFormat(int a_format) const {
   throw scError("Unknown address format ["+toString(a_format)+"]");
}
   
scString scMessageAddress::buildAddress() const
{
  scString res;
  scString resPart;
  
  if (!m_protocol.empty()) 
  { 
    res = m_protocol+"::";  
  }
  
  switch (m_format)
  {
  case AdrFmtVPath: 
    res += m_vpath;
    break;
  case AdrFmtRaw: 
    res += m_rawAddress;
    break;
  case AdrFmtRole: 
    res += "@"+m_role;
    break;
  case AdrFmtDefault:  
    resPart = m_host;
    if (!m_node.empty())
      resPart += "/"+m_node;
    if (!m_task.empty())
      resPart += "/"+m_task;
    res += "#"+resPart;
    break;
  default:      
    throwUnknownFormat(m_format);
    break;
  } // switch context   

  return res;
}

bool scMessageAddress::isRoleName(const scString &address)
{
  return ((address.size() > 1) && (address.find("@") == 0));
}

scString scMessageAddress::buildRoleName(const scString &serviceName)
{
    if (serviceName.empty())
        return "";
    else
        return scString("@") + serviceName;
}

bool scMessageAddress::isSpecial(int c)
{
  switch (c)
  {
  case '@': case '#': case '/': case ':':
    return true;
  default:
    return false;
  }
}

bool scMessageAddress::isAscii(int c)
{
  return c >= 32 && c <= 127;
}

bool scMessageAddress::isCtl(int c)
{
  return c > 0 && c <= 31 || c == 127;
}
