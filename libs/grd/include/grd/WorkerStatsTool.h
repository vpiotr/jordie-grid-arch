/////////////////////////////////////////////////////////////////////////////
// Name:        WorkerStatsTool.h
// Project:     grdLib
// Purpose:     Tool for statistics collection
// Author:      Piotr Likus
// Modified by:
// Created:     30/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDWORKSTATTOOL_H__
#define _GRDWORKSTATTOOL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file WorkerStatsTool.h
\brief Short file description

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "dtp/dnode.h"
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

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scWqWorkerStatsTool {
public:
  static void resetTimers(const scDataNode &statFilterList, scDataNode &statBackup);
  static void resetCounters(const scDataNode &statFilterList, scDataNode &statBackup);
  static void prepareTimerStats(const scDataNode &statFilterList, scDataNode &output);
  static void restoreTimerStats(const scDataNode &values);
  static void prepareCounterStats(const scDataNode &statFilterList, scDataNode &output);
  static void removeEmptyStats(scDataNode &output);
  static void restoreCounterStats(const scDataNode &values);
};


#endif // _GRDWORKSTATTOOL_H__