/////////////////////////////////////////////////////////////////////////////
// Name:        WorkQueueWorker.cpp
// Project:     grdLib
// Purpose:     Worker part of work queue handling.
// Author:      Piotr Likus
// Modified by:
// Created:     30/08/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/WorkQueueWorker.h"
#include "sc/proc/process.h"
#include "perf/Timer.h"
#include "perf/Counter.h"
#include "perf/Log.h"

using namespace perf;

void scWqModalWorker::addWorkerIdToResult(scDataNode &output)
{
  if (!output.isParent())
    output.setAsParent();
  output.setElementSafe("_worker_id", scDataNode(static_cast<ulong64>(proc::getCurrentProcessId())));  
  //Log::addDebug("scWqModalWorker::addWorkerIdToResult - end");
}

