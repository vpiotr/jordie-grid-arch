/////////////////////////////////////////////////////////////////////////////
// Name:        MessageTrace.cpp
// Project:     grdLib
// Purpose:     Writes details of processing of messages to special log file.
// Author:      Piotr Likus
// Modified by:
// Created:     12/02/2011
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd\MessageTrace.h"

#include "base\date.h"

#include "perf\time_utils.h"

#include "sc\proc\process.h"
#include "sc\defs.h"

using namespace perf;
using namespace dtp;
using namespace proc;

scMessageTrace::scMessageTrace(perf::LogDevice &logDevice): m_traceMsgCode(LOG_MSG_TRACE), m_logDevice(&logDevice)
{
}

scMessageTrace::~scMessageTrace()
{
}

void scMessageTrace::addTraceHeader()
{
  dtpString line = "when;uptime_ms;proc_id;line_id;msg_order;event_code;sender;receiver;msg_id;command;timediff";
  Log::addText(line, lmlTrace, LOG_MSG_TRACE);
}

int scMessageTrace::getMessageOrder(const dtpString &eventCode)
{
  int order;

  if (eventCode == "req_send_prep")
    order = 1;
  else if (eventCode == "req_rdy")
    order = 2;
  else if (eventCode == "gate_req_rdy") 
    order = 3;
  else if (eventCode == "gate_req_sent")
    order = 4;
  else if (eventCode == "gate_req_recv")
    order = 5;
  else if (eventCode == "req_recv")
    order = 6;
  else if (eventCode == "resp_rdy")
    order = 7;
  else if (eventCode == "resp_put")
    order = 8;
  else if (eventCode == "gate_resp_rdy")
    order = 9;
  else if (eventCode == "gate_resp_sent")
    order = 10;
  else if (eventCode == "gate_resp_recv")
    order = 11;
  else if (eventCode == "resp_recv_ok")
    order = 12;
  else
    order = 100;

  return order;
}

void scMessageTrace::addTrace(const dtpString &eventCode, const dtpString &sender, const dtpString &receiver, int msgId, const dtpString &command, int sortOrder)
{
  dtp::dnode dets(ict_parent);

  int realOrder;

  if (sortOrder >= 0)
    realOrder = sortOrder;
  else
    realOrder = getMessageOrder(eventCode);

  dets.addChild("order", realOrder);
  dets.addChild("event", eventCode);
  dets.addChild("sender", sender);
  dets.addChild("receiver", receiver);
  dets.addChild("msg_id", msgId);
  dets.addChild("command", command);

  addTrace(dets);
}

void scMessageTrace::addStep(const dtpString &eventCode, const dtpString &section, int sortOrder)
{
  dtp::dnode dets(ict_parent);

  dets.addChild("order", sortOrder);
  dets.addChild("event", eventCode);
  dets.addChild("sender", "x");
  dets.addChild("receiver", "x");
  dets.addChild("msg_id", 0);
  dets.addChild("command", section);

  addTrace(dets);
}

//void scMessageTrace::addTrace(const dtpString &msgText)
//{
//  dtp::dnode dets(ict_parent);
//  addTrace(msgText, dets);
//}

void scMessageTrace::addTrace(const dtp::dnode &fields)
{
  static uint64 priorUptime = 0;
  static uint64 lineId = 0;

  dtp::dnode allFields(dtp::ict_parent);
  dtp::dnode textNode;

  textNode = dateTimeToIsoStr(currentDateTime());
  allFields.push_back("when", textNode);
  uint64 uptime = os_uptime_ms();
  uint64 loc_time = cpu_time_ticks();
  allFields.push_back("uptime_ms", uptime);

  uint64 timeDiff;

  if (priorUptime > 0) 
    timeDiff = loc_time - priorUptime;
  else
    timeDiff = 0;

  lineId++;

  textNode = static_cast<uint64>(getCurrentProcessId());
  allFields.push_back("proc_id", textNode);
  
  allFields.push_back("line_id", lineId);

  for(uint i=0, epos = fields.size(); i != epos; i++)
    allFields.push_back(fields.getElementName(i), fields.getElement(i));

  allFields.push_back("timediff", timeDiff);

  dtpString finalLine = allFields.implode(";");

  Log::addText(finalLine, lmlTrace, LOG_MSG_TRACE);

  priorUptime = loc_time;
}

void scMessageTrace::intAddText(const dtpString &a_text, LogMsgLevel level, uint msgCode)
{
  if ((msgCode == m_traceMsgCode) && (level == lmlTrace)) {
    //Log::intAddText(a_text, level, msgCode);
    dtpString msg;
    formatMessage(msg, a_text, level, toString(msgCode));
    m_logDevice->intAddText(a_text);
  }
}

void scMessageTrace::intAddText(const dtpString &a_text)
{
  m_logDevice->intAddText(a_text);
}

void scMessageTrace::intFlush()
{
  m_logDevice->intFlush();
}

void scMessageTrace::formatMessage(dtpString &output, const dtpString &a_text, LogMsgLevel level, const dtpString &msgCode)
{
  output = a_text;
}


