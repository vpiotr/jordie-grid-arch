/////////////////////////////////////////////////////////////////////////////
// Name:        MessageAddress.h
// Project:     grdLib
// Purpose:     Message address container
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMSGADDR_H__
#define _GRDMSGADDR_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file MessageAddress.h
\brief Message address container

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
/// special addresses for messages
const scString SC_ADDR_ALL      = "@all";   ///< all registered nodes
const scString SC_ADDR_THIS     = "@this";  ///< only for the same node
const scString SC_ADDR_LOCAL    = "@local"; ///< all local (same computer) nodes
const scString SC_ADDR_NULL     = "@null";  ///< null queue (delete message)
const scString SC_ADDR_LOG      = "@log";  ///< logging node
const scString SC_ADDR_CONTROL  = "@control";  ///< GUI/control node

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// scMessageAddress
// ----------------------------------------------------------------------------
/// \brief Address for message
/// \details
/// Address format: 
///   [protocol::][fixed-path|virtual-path|role]
///
/// role: @role-name
///   - multi-node name
///
/// fixed-path: #[computer]/[node-id]/[task-id] e.g. #/A123/
///   - can be parset
///
/// virtual-path: //path/to/node e.g. //A123/X11/a/bb...
///   - cannot be parsed
/// 
/// raw: unknown format
///   - cannot be parsed
/// 
class scMessageAddress
{
public:
    enum scAddressFormat {AdrFmtDefault, AdrFmtVPath, AdrFmtRole, AdrFmtRaw};

    scMessageAddress();
    scMessageAddress( const scString& src);
    scMessageAddress( const scMessageAddress& rhs); 
    scMessageAddress& operator=( const scMessageAddress& rhs);    
    
    virtual ~scMessageAddress();

    void clear();    
    void set(const scString &address);

    scString getProtocol() const;
    scString getHost() const;
    scString getNode() const;
    scString getTask() const;
    scString getRole() const;
    scString getPath() const;
    scAddressFormat getFormat() const;
    void setProtocol(const scString &a_protocol);
    void setHost(const scString &a_value);
    void setNode(const scString &a_value);
    void setTask(const scString &a_value);

    void setAsString(const scString &value);
    scString getAsString() const;
    virtual bool isEmpty(); 
    
    virtual void parseAddress(const scString &address);    
    virtual scString buildAddress() const;
    static bool isRoleName(const scString &address);
    static scString buildRoleName(const scString &serviceName);
protected:
    void throwAddressError(const scString &address, int a_pos, int a_char) const;
    void throwUnknownFormat(int a_format) const;
    bool isSpecial(int c);        
    bool isAscii(int c);
    bool isCtl(int c);
private:
    scString m_protocol;
    scString m_host;
    scString m_node;
    scString m_task; 
    scString m_role; 
    scString m_vpath; 
    scString m_rawAddress;
    scAddressFormat m_format;
};


#endif // _GRDMSGADDR_H__