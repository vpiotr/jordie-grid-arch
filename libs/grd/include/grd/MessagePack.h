/////////////////////////////////////////////////////////////////////////////
// Name:        MessagePack.h
// Project:     scLib
// Purpose:     Message pack object. Used to handle multiple messages in 
//              the same time.
// Author:      Piotr Likus
// Modified by:
// Created:     24/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _MESSAGEPACK_H__
#define _MESSAGEPACK_H__

#include "grd/core.h"

// ----------------------------------------------------------------------------
// scMessagePack
// ----------------------------------------------------------------------------  
/// Group of messages sent and received in parallel
class scMessagePack: public scRequestHandlerBox 
{
public:
  scMessagePack();
  virtual ~scMessagePack();
  virtual bool isResponseReady() const;
  virtual bool isAllHandled() const;
  scResponse getLastResponse() const;
  void getFullResult(scDataNode &result);
  void addMessage(const scString &address, const scString &command, 
      const scDataNode *params = SC_NULL, 
      int requestId = SC_REQUEST_ID_NULL);
  void addEnvelope(scEnvelope *a_envelope);    
  void post(scScheduler *a_scheduler);
  /// returns <true> if whole result is OK
  bool isResultOK() const;
  /// returns <true> if object is waiting for any responses
  bool isWaiting() const;
  void clear();
  //--- properties ---
  void setChunkCount(int a_value);
  int getChunkCount() const;
  void setSplitVarName(const scString &a_name);
  scString getSplitVarName() const;
protected:  
  virtual void beforeReqQueued(const scEnvelope &a_envelope);
  virtual void handleReqResult(const scMessage &a_message, const scResponse &a_response);
  virtual void handleReqError(const scMessage &a_message, const scResponse &a_response);
  virtual void handleAllReceived();
  void checkThatsAll();
  virtual bool splitEnvelope(const scEnvelope *a_envelope);
  void doAddEnvelope(scEnvelope *a_envelope);
  bool splitParam(const scDataNode &a_params, const scString &a_varName, scDataNode &outputParam);  
  void splitParamAsSet(const scDataNode &paramForSplit, const scString &a_varName, scDataNode &outputParam);
  void splitParamAsRange(const scDataNode &paramForSplit, const scString &a_varName, scDataNode &outputParam);
  void splitParamAsArray(const scDataNode &paramForSplit, const scString &a_varName, scDataNode &outputParam);
protected:  
  scEnvelopeColn m_waiting;
  scEnvelopeColn m_sent;
  scEnvelopeColn m_received;
  scString m_splitVarName; ///< can be used to split message by selected param
  int m_errorCount;
  int m_receivedCount; ///< can be greater than m_received.size() when msgs splitted by address
  int m_sentCount;
  int m_chunkCount;
};

#endif _MESSAGEPACK_H__