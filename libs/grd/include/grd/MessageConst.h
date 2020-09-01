/////////////////////////////////////////////////////////////////////////////
// Name:        MessageConst.h
// Project:     grdLib
// Purpose:     Message handling constants
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMSGCONST_H__
#define _GRDMSGCONST_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file MessageConst.h
\brief Short file description

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// #include ".."

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

/// message handling statuses (0: OK, >0: warning, <0: error)
const int SC_MSG_STATUS_OK          = 0;  ///< message handled correctly
const int SC_MSG_STATUS_PASS        = 1;  ///< handled, pass to next handler
const int SC_MSG_STATUS_TASK_REQ    = 2;  ///< create task for this message
const int SC_MSG_STATUS_FORWARDED   = 3;  ///< message forwarded, do not return response
const int SC_MSG_STATUS_UNK_MSG     = -1; ///< unknown message, pass to others
const int SC_MSG_STATUS_ERROR       = -2; ///< unknown error
const int SC_MSG_STATUS_EXCEPTION   = -3; ///< unknown error (exception)
const int SC_MSG_STATUS_WRONG_PARAMS= -4; ///< unknown error (exception)
const int SC_MSG_STATUS_OVERFLOW    = -5; ///< message capacity reached
const int SC_MSG_STATUS_UNK_TASK    = -6; ///< unknown task
const int SC_MSG_STATUS_MSG_ID_REQ  = -7; ///< message ID required
const int SC_MSG_STATUS_WAITING     = -8; ///< waiting for result
const int SC_MSG_STATUS_USR_ABORT   = -9; ///< user-aborted
const int SC_MSG_STATUS_WRONG_CFG   = -10; ///< wrong system configuration

// response status transported through gates, includes SC_MSG_STATUS
const int SC_RESP_STATUS_OK              = 0;
const int SC_RESP_STATUS_BASE            = -100;
const int SC_RESP_STATUS_HND_BASE        = -1;    ///< base for handler-provided statuses
const int SC_RESP_STATUS_USER_BASE       = -1000; ///< base for user-defined statuses
const int SC_RESP_STATUS_UNDEF_ERROR     = SC_RESP_STATUS_BASE-1;
const int SC_RESP_STATUS_UNKNOWN_NODE    = SC_RESP_STATUS_BASE-2;
const int SC_RESP_STATUS_TRANSMIT_ERROR  = SC_RESP_STATUS_BASE-3;
const int SC_RESP_STATUS_TIMEOUT         = SC_RESP_STATUS_BASE-4;
const int SC_RESP_STATUS_RETRY_OVERFLOW  = SC_RESP_STATUS_BASE-5;

#endif // _GRDMSGCONST_H__