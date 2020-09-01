/////////////////////////////////////////////////////////////////////////////
// Name:        Message.h
// Project:     grdLib
// Purpose:     Message container for a request sent to processor
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMSG_H__
#define _GRDMSG_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Message.h
\brief Message container for a request sent to processor

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/Event.h"

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
// scMessage
// ----------------------------------------------------------------------------
/// A request sent to processor
class scMessage: public scEvent
{
public:
    scMessage();
    scMessage(const scString &command, 
      const scDataNode *a_params, 
      const int requestId);
    scMessage(scMessage const &rhs);    
    scMessage& operator=( const scMessage& rhs);
    virtual ~scMessage(){};
    virtual scEvent *clone() const;
    /// returns full command
    scString getCommand() const;
    /// returns command without inteface (for commands like "interface.command")
    scString getCoreCommand() const;
    /// returns interface from command name (for commands like "interface.command")
    scString getInterface() const;
    scDataNode &getParams();
    virtual void clear();
    void setCommand(const scString &a_command);
    void setParams(const scDataNode &a_params);
    bool hasParams() const;
    bool hasRequestId() const;
protected:
    void copyFrom(const scMessage& rhs);
protected:
    scString m_command;
    scDataNode m_params;
};


#endif // _GRDMSG_H__