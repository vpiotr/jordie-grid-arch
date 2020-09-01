/////////////////////////////////////////////////////////////////////////////
// Name:        Response.h
// Project:     grdLib
// Purpose:     Response event container.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDRESP_H__
#define _GRDRESP_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file Response.h
\brief Response event container.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "base/utils.h"
#include "sc/dtypes.h"
#include "grd/Event.h"
#include "grd/Message.h"

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
// scResponse
// ----------------------------------------------------------------------------
/// Response for the sent request
class scResponse: public scEvent
{
public:
    scResponse();
    scResponse(scResponse const &rhs);    
    scResponse& operator=( const scResponse& rhs);
    virtual ~scResponse();
    virtual scEvent *clone() const;
    // properties
    void setStatus(int a_status);
    void setResultNull();
    void setResult(const scDataNode &a_result);
    void setResult(base::move_ptr<scDataNode> a_result);
    void setErrorNull();
    void setError(const scDataNode &a_error);
    int getStatus() const;
    scDataNode &getResult() const;
    scDataNode &getError() const;
    virtual bool isResponse() const;    
    virtual bool isError() const;
    void clearResult();
    virtual void clear();
    virtual void initFor(const scMessage &src);
protected:
    int m_status;
    scDataNode m_result;
    scDataNode m_error;
};


#endif // _GRDRESP_H__