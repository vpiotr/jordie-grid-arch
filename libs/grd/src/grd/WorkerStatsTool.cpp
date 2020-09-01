/////////////////////////////////////////////////////////////////////////////
// Name:        WorkerStatsTool.h
// Project:     grdLib
// Purpose:     Tool for statistics collection
// Author:      Piotr Likus
// Modified by:
// Created:     30/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/WorkerStatsTool.h"
#include "perf/Timer.h"
#include "perf/Counter.h"
#include "perf/Log.h"

using namespace perf;

void scWqWorkerStatsTool::resetTimers(const scDataNode &statFilterList, scDataNode &statBackup)
{
  scDataNode timers;
  Timer::getByFilter(statFilterList, timers, tfsStopped);
  
  for(uint i=0, epos = timers.size(); i != epos; i++)
    Timer::reset(timers.getElementName(i));

  statBackup.setElementSafe("timers", base::move<scDataNode>(timers)); 
}

void scWqWorkerStatsTool::resetCounters(const scDataNode &statFilterList, scDataNode &statBackup)
{
  scDataNode counters;
  Counter::getByFilter(statFilterList, counters);

  for(uint i=0, epos = counters.size(); i != epos; i++)
    Counter::reset(counters.getElementName(i));

  statBackup.setElementSafe("counters", base::move<scDataNode>(counters)); 
}

void scWqWorkerStatsTool::prepareTimerStats(const scDataNode &statFilterList, scDataNode &output)
{
  Timer::getByFilter(statFilterList, output, tfsStopped);
}

void scWqWorkerStatsTool::restoreTimerStats(const scDataNode &values)
{
  scString valueName;
  for(uint i=0, epos = values.size(); i != epos; i++)
  {
    valueName = values.getElementName(i);
    Timer::reset(valueName);
    Timer::inc(valueName, values.getUInt64(valueName));
  }
}

void scWqWorkerStatsTool::prepareCounterStats(const scDataNode &statFilterList, scDataNode &output)
{
  Counter::getByFilter(statFilterList, output);
}

//void scWqWorkerStatsTool::removeEmptyStats(scDataNode &output)
//{
//  scDataNode valueBackup(output);
//  output.clear();
//
//  for(uint i=0, epos = valueBackup.size(); i != epos; i++)
//  {
//    if (valueBackup.getUInt64(i) > 0)
//      output.addChild(valueBackup.getElementName(i), new scDataNode(valueBackup[i]));
//  }
//}

void scWqWorkerStatsTool::removeEmptyStats(scDataNode &output)
{
  uint i = output.size();
  while(i > 0) {
    i--;
    if (output.getUInt64(i) == 0)
      output.eraseElement(i);
  }
}

void scWqWorkerStatsTool::restoreCounterStats(const scDataNode &values)
{
  //Log::addDebug(scString("restoreCounterStats.beg - gx_ceval = ")+toString(Counter::getTotal("gx-ceval")));
  scString valueName;
  for(uint i=0, epos = values.size(); i != epos; i++)
  {
    valueName = values.getElementName(i);
    Counter::reset(valueName);
    Counter::inc(valueName, values.getUInt64(valueName));
  }
  //Log::addDebug(scString("restoreCounterStats.end - gx_ceval = ")+toString(Counter::getTotal("gx-ceval")));
}
