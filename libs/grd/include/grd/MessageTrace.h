/////////////////////////////////////////////////////////////////////////////
// Name:        MessageTrace.h
// Project:     grdLib
// Purpose:     Writes details of processing of messages to special log file.
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDMSGTRACE_H__
#define _GRDMSGTRACE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file MessageTrace.h
\brief Writes details of processing of messages to special log file.

Usage:
  Log::logTrace("test message", fields);

*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "perf\FileLog.h"
#include "perf\Log.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const uint LOG_MSG_TRACE = 4000;

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scMessageTrace: public perf::LogDevice
{
  typedef perf::LogDevice inherited;
public:
  scMessageTrace(perf::LogDevice &logDevice);
  virtual ~scMessageTrace();
  static void addTrace(const dtpString &eventCode, const dtpString &sender, const dtpString &receiver, int msgId, const dtpString &command, int sortOrder = -1);
  //static void addTrace(const dtpString &msgText);
  static void addTrace(const dtp::dnode &fields);
  static void addTraceHeader();
  static void addStep(const dtpString &eventCode, const dtpString &section, int sortOrder);
protected:
  virtual void formatMessage(dtpString &output, const dtpString &a_text, perf::LogMsgLevel level, const dtpString &msgCode);
  virtual void intAddText(const dtpString &a_text, perf::LogMsgLevel level, uint msgCode);
  virtual void intAddText(const dtpString &a_text);
  static int getMessageOrder(const dtpString &eventCode);
  virtual void intFlush();
private:
  uint m_traceMsgCode;
  perf::LogDevice *m_logDevice;
};

#endif // _GRDMSGTRACE_H__