/////////////////////////////////////////////////////////////////////////////
// Name:        EnvSerializerJsonYajl.cpp
// Project:     scLib
// Purpose:     Serializer using JSON format & Yajl lib
// Author:      Piotr Likus
// Modified by:
// Created:     19/11/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//#define SC_ESJSONY_LOG_ENABLED    
//#define SC_ESJSONY_LOG_PARSER    

// sc
#include "sc/dtypes.h"

// dtp
#include "dtp/YawlIoClasses.h"

// grd
#include "grd/EnvSerializerJsonYajl.h"
#include "grd/Response.h"
#include "grd/Message.h"

#ifdef SC_TIMER_ENABLED
#include "perf/Timer.h"
#endif

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace dtp;
using namespace perf;

#define SC_STRING_TO_UCHAR(a) reinterpret_cast<unsigned char *>(const_cast<wxChar *>((a)))


// ----------------------------------------------------------------------------
// scEnvSerializerJsonYajl
// ----------------------------------------------------------------------------
scEnvSerializerJsonYajl::scEnvSerializerJsonYajl()
{
}

int scEnvSerializerJsonYajl::convToString(const scEnvelope& input, scString &output)
{
#ifdef SC_TIMER_ENABLED
  Timer::start("JSONy.Out.01.ToString");
#endif

  YawlWriter writer(false, SC_NULL);
  yajl_gen *ctx = writer.getContext();

  yajl_gen_map_open(*ctx); 
  writer.writeAttrib("sender", input.getSender().getAsString());
  writer.writeAttrib("receiver", input.getReceiver().getAsString());
  if (input.getTimeout() != 0)
    writer.writeAttrib("timeout", (int)input.getTimeout());
  if (input.getEvent() != SC_NULL)
  {    
    writer.startAttrib("event");
    yajl_gen_map_open(*ctx); 
    
    if (input.getEvent()->isResponse())
      writer.writeAttrib("is_response", true);
    
    if (input.getEvent()->getRequestId() != SC_REQUEST_ID_NULL)
      writer.writeAttrib("request_id", input.getEvent()->getRequestId());

    if (input.getEvent()->isResponse())
    {
      scResponse *response = dynamic_cast<scResponse *>(input.getEvent());
      writer.writeAttrib("status", (int)response->getStatus());
      if (response->isError())
      {
        writer.startAttrib("error");
        writer.writeDataNode(response->getError());
      } else {
        if (!response->getResult().isNull()) {
          writer.startAttrib("result");
          writer.writeDataNode(response->getResult());
        }  
      }
    } else { // message
      scMessage *message = dynamic_cast<scMessage *>(input.getEvent());
      writer.writeAttrib("command", message->getCommand());
      writer.startAttrib("params");
      writer.writeDataNode(message->getParams());
    }    
    
    //close event
    yajl_gen_map_close(*ctx);     
  }
    
  yajl_gen_map_close(*ctx); 

#ifdef SC_TIMER_ENABLED
  Timer::start("JSONy.Out.02.exportString");
#endif
  writer.outputToString(output);
#ifdef SC_TIMER_ENABLED
  Timer::stop("JSONy.Out.02.exportString");
#endif

#ifdef SC_TIMER_ENABLED
  Timer::stop("JSONy.Out.01.ToString");
#endif
  
#ifdef SC_ESJSONY_LOG_ENABLED  
  scLog::addText("JSON message send: "+output);
#endif
  
  return 0;
}

int scEnvSerializerJsonYajl::convFromString(const scString &input, scEnvelope& output)
{
  scDataNode workNode;
  YawlReaderForDataNode reader(false, false);
  int res = 1;

#ifdef SC_ESJSONY_LOG_ENABLED  
  scLog::addText("JSON message recv: "+input);
#endif

#ifdef SC_TIMER_ENABLED
  Timer::start("JSONy.In.01.FromString");
#endif
  
#ifdef SC_ESJSONY_LOG_ENABLED  
  try {      
#endif
#ifdef SC_TIMER_ENABLED
  Timer::start("JSONy.In.02.parse");
#endif

  if (!reader.parseString(input, workNode))
  {
    res = 0;
  }
#ifdef SC_TIMER_ENABLED
  Timer::stop("JSONy.In.02.parse");
#endif
  
#ifdef SC_TIMER_ENABLED
  Timer::start("JSONy.In.03.node2env");
#endif
  if (res && !convDataNodeToEnvelope(workNode, output))
  {
    res = 0;
  } 
#ifdef SC_TIMER_ENABLED
  Timer::stop("JSONy.In.03.node2env");
#endif

#ifdef SC_TIMER_ENABLED
  Timer::stop("JSONy.In.01.FromString");
#endif

#ifdef SC_ESJSONY_LOG_ENABLED  
  } // try
  catch (const std::exception& e) {
    scLog::addError("scEnvSerializerJsonYajl::convFromString - exception: "+scString(e.what()));
    throw;
  }    
#endif
    
  return res;
}

int scEnvSerializerJsonYajl::convDataNodeToEnvelope(const scDataNode &input, scEnvelope& output)
{
  output.clear();
  if (input.hasChild("sender"))
    output.setSender(const_cast<scDataNode &>(input)["sender"].getAsString());
    
  if (input.hasChild("receiver"))
    output.setReceiver(const_cast<scDataNode &>(input)["receiver"].getAsString());

  if (input.hasChild("timeout"))
    output.setTimeout(const_cast<scDataNode &>(input)["timeout"].getAsInt());
    
  if (input.hasChild("event"))
  {
    scDataNode &eventNode = const_cast<scDataNode &>(input)["event"];
    bool bResponse = false;
    if (eventNode.hasChild("is_response"))
      bResponse = eventNode["is_response"].getAsBool();
    int requestId = SC_REQUEST_ID_NULL;  
    if (eventNode.hasChild("request_id"))
      requestId = eventNode["request_id"].getAsInt();
    
    if (bResponse) {
    // response
      std::auto_ptr<scResponse> guard(new scResponse());      
      scResponse *response = guard.get();
      response->setRequestId(requestId);
      int status = eventNode["status"].getAsInt();
      response->setStatus(status);
      if (response->isError())
      {
        if (eventNode.hasChild("error"))
        {
          //response->setError(eventNode["error"]);
          response->getError().eatValueFrom(eventNode["error"]);                  
        }  
      } else {
        if (eventNode.hasChild("result"))
        {
        //  response->setResult(scDataNode("result",2));
          //response->setResult(eventNode["result"]);
          response->getResult().eatValueFrom(eventNode["result"]);        
        }  
      }
      output.setEvent(guard.release());
    } else { 
    // message  
      std::auto_ptr<scMessage> guard(new scMessage());      
      scMessage *message = guard.get();
      message->setRequestId(requestId);

      message->setCommand(eventNode["command"].getAsString());

      if (eventNode.hasChild("params"))
      {
        message->setParams(eventNode["params"]);
      }  
      
      output.setEvent(guard.release());
    }    
  } else {
    throw scError("Invalid envelope - no event found"); 
  }
  
  return 1;    
}
