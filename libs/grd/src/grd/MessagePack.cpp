/////////////////////////////////////////////////////////////////////////////
// Name:        MessagePack.cpp
// Project:     scLib
// Purpose:     Multi-message pack handling
// Author:      Piotr Likus
// Modified by:
// Created:     24/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "sc/log.h"

#include "grd/MessagePack.h"

// ----------------------------------------------------------------------------
// scMessagePack
// ----------------------------------------------------------------------------  
scMessagePack::scMessagePack()
{
  m_errorCount = m_sentCount = m_receivedCount = 0;
  m_chunkCount = 1;
}

scMessagePack::~scMessagePack()
{
}

void scMessagePack::setChunkCount(int a_value)
{
  m_chunkCount = a_value;
}
  
int scMessagePack::getChunkCount() const
{
  return m_chunkCount;
}

void scMessagePack::setSplitVarName(const scString &a_name)
{
  m_splitVarName = a_name;
}

scString scMessagePack::getSplitVarName() const
{
  return m_splitVarName;
}

bool scMessagePack::isResponseReady() const
{
  return ((m_sentCount > 0) && (m_receivedCount >= m_sentCount));
//  return ((m_sent.size()>0) && (m_sent.size() == m_received.size()));
}

bool scMessagePack::isAllHandled() const
{
  return (m_waiting.size() == 0) && ((m_sentCount == 0) || isResponseReady());
}

void scMessagePack::clear()
{
  m_errorCount = m_receivedCount = m_sentCount = 0;
  m_waiting.clear();
  m_sent.clear();
  m_received.clear();
  scRequestHandlerBox::clear();
}

scResponse scMessagePack::getLastResponse() const 
{
  return getResponse();
}

bool scMessagePack::isResultOK() const
{
  return (m_errorCount <= 0);
}

bool scMessagePack::isWaiting() const
{
  return ((m_sentCount > 0) && (m_receivedCount < m_sentCount));
  //return ((m_sent.size()>0) && (m_sent.size() > m_received.size()));
}

void scMessagePack::getFullResult(scDataNode &result)
{
  scResponse *response;
  scDataNode *newResult;
  std::auto_ptr<scDataNode> resultGuard;
  
/*  
  scLog::addText("pack: start");
  int debugCnt = 1;
  for(scEnvelopeColnIterator i=m_received.begin(); i!=m_received.end(); ++i){
    response = dynamic_cast<scResponse *>(i->getEvent());
    if (response != SC_NULL)
    {
      scLog::addText("pack: received "+toString(debugCnt++)+", result -");
      scLog::addText(response->getResult().dump());
    }  
   }
  scLog::addText("pack: end");
*/ 
  for(scEnvelopeColnIterator i=m_received.begin(); i!=m_received.end(); ++i){
    response = dynamic_cast<scResponse *>(i->getEvent());
    if (response != SC_NULL)
    {
      if (response->isError())
      {
        result.addChild(new scDataNode());
      } else { // result OK
        resultGuard.reset(new scDataNode());
        newResult = resultGuard.get();
        newResult->copyValueFrom(
          response->getResult()
        );  
        result.addChild(resultGuard.release());
//        scLog::addText("pack: passed "+toString(debugCnt++)+" messages");
      }  
    }
  }    
}

void scMessagePack::addMessage(const scString &address, const scString &command, 
    const scDataNode *params, 
    int requestId)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope());
  scEnvelope *newEnvelope = envelopeGuard.get();
  newEnvelope->setReceiver(scMessageAddress(address));
  std::auto_ptr<scMessage> messageGuard(new scMessage(command, params, requestId));
  newEnvelope->setEvent(messageGuard.release());
  addEnvelope(envelopeGuard.release());
}

/// eats argument
void scMessagePack::addEnvelope(scEnvelope *a_envelope)
{
  bool added = false;
  
  if (m_chunkCount > 1)
  {
    if (splitEnvelope(a_envelope))
    {
      delete a_envelope;
      added = true;
    }
  }
  
  if (!added)    
    doAddEnvelope(a_envelope);
}

void scMessagePack::doAddEnvelope(scEnvelope *a_envelope)
{
  m_waiting.push_back(a_envelope);
}

void scMessagePack::post(scScheduler *a_scheduler)
{
  scEnvelopeColn toBeSent(m_waiting);
  scEnvelopeTransport transp;
  
  while(!toBeSent.empty()) 
  {
    transp = toBeSent.pop_front();
    if (transp->getEvent()->getRequestId() == SC_REQUEST_ID_NULL)
      transp->getEvent()->setRequestId(a_scheduler->getNextRequestId());
    //m_sent.push_back(new scEnvelope(*transp.get())); 
    scEnvelope *orgEnvelope = transp.get();         
    m_sent.push_back(orgEnvelope);
    a_scheduler->postEnvelope(new scEnvelope(*transp.release()), this);            
  }  

  m_waiting.clear();
}

void scMessagePack::beforeReqQueued(const scEnvelope &a_envelope)
{
  ++m_sentCount;
}

void scMessagePack::handleReqResult(const scMessage &a_message, const scResponse &a_response)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope());
  scEnvelope *newEnvelope = envelopeGuard.get();
  newEnvelope->setEvent(new scResponse(a_response)); 
  m_received.push_back(envelopeGuard.release());
  ++m_receivedCount;
  checkThatsAll();
}

void scMessagePack::handleReqError(const scMessage &a_message, const scResponse &a_response)
{
  std::auto_ptr<scEnvelope> envelopeGuard(new scEnvelope());
  scEnvelope *newEnvelope = envelopeGuard.get();
  newEnvelope->setEvent(new scResponse(a_response)); 
  m_received.push_back(envelopeGuard.release());
  ++m_receivedCount;
  ++m_errorCount;
  scLog::addError("scMessagePack::handleReqError: "+a_response.getError().getString("text", ""));
    
  checkThatsAll();
}

void scMessagePack::checkThatsAll()
{
  if (isResponseReady())  
    handleAllReceived();
}

void scMessagePack::handleAllReceived()
{ // do nothing here
}

/// Split message by m_splitVarName or by row set
bool scMessagePack::splitEnvelope(const scEnvelope *a_envelope)
{
  bool res = false;
  bool splitByVar = (m_splitVarName.length() > 0);
  
  scMessage *message = dynamic_cast<scMessage *>(a_envelope->getEvent());

  if (message->getParams().isParent() || message->getParams().isArray())
  {
    std::auto_ptr<scDataNode> paramGuard(new scDataNode());
    scDataNode *splittedParam = paramGuard.get();

    if (splitParam(message->getParams(), m_splitVarName, *splittedParam))
    if (splittedParam->isParent() && !splittedParam->empty())
    {
        std::auto_ptr<scEnvelope> envelopeGuard;
        scEnvelope *partEnvelope;
        scMessage *partMessage;
        scEnvelope tplEnvelope = *a_envelope;
        scMessage *tplMessage = dynamic_cast<scMessage *>(tplEnvelope.getEvent());
        
        res = true;
        
        if (tplMessage) 
        {
          if (splitByVar) {
            tplMessage->getParams()[m_splitVarName].clear();
          } else { 
            tplMessage->getParams().clear();
          }
        }  
        
        scDataNode::const_iterator start = splittedParam->begin();
        scDataNode::const_iterator end = splittedParam->end();
        
        for(scDataNode::const_iterator p = start;
            p != end; 
            ++p)  
        {   
          envelopeGuard.reset(new scEnvelope(tplEnvelope));
          partEnvelope = envelopeGuard.get();
          partMessage = dynamic_cast<scMessage *>(partEnvelope->getEvent());
          assert(partMessage != SC_NULL);          
          if (splitByVar) {
            partMessage->getParams()[m_splitVarName] = (p->getAsElement());
          } else { 
            partMessage->getParams() = (p->getAsElement());
          }  
          doAddEnvelope(envelopeGuard.release());
        } // for  
    }
  }  
  
  return res;
}

bool scMessagePack::splitParam(const scDataNode &a_params, const scString &a_varName, scDataNode &outputParam)
{
  bool res = false;
  if ((!a_params.empty()) && (getChunkCount() > 1))
  {    
    if (!a_varName.length() || a_params.hasChild(a_varName))
    {
      scDataNode &paramForSplit = (a_varName.length()?const_cast<scDataNode &>(a_params)[a_varName]:const_cast<scDataNode &>(a_params));
      
      if (paramForSplit.isArray()) {
        splitParamAsArray(paramForSplit, a_varName, outputParam);
      } else { // split by set
        splitParamAsSet(paramForSplit, a_varName, outputParam);
      } // split by set
      res = true;
    } // var param OK
  } // children and chunk count OK
  return res;
}

void scMessagePack::splitParamAsSet(const scDataNode &paramForSplit, const scString &a_varName, scDataNode &outputParam)
{
  int childCount = const_cast<scDataNode &>(paramForSplit).size();
  int partCount;
  if (childCount < getChunkCount())
    partCount = childCount;
  else
    partCount = getChunkCount();
    
  int partSize;

  if (partCount >= 1)
    partSize = int(float(childCount) / float(partCount));  
  else
    partSize = 1;
      
  if (partSize < 1)
    partSize = 1;
    
  std::auto_ptr<scDataNode> paramGuard;
  scDataNode *newPart;
  int addedCount = 0, currPartCount;
  
  while (addedCount < childCount) 
  {
    paramGuard.reset(new scDataNode());
    newPart = paramGuard.get();
    currPartCount = 0;
    while((currPartCount < partSize) && (addedCount + currPartCount < childCount))
    {
      newPart->addChild(new scDataNode(const_cast<scDataNode &>(paramForSplit).getChildren().at(addedCount + currPartCount)));    
      ++currPartCount;
    }
    outputParam.addChild(a_varName, paramGuard.release());
    addedCount += currPartCount;
  } // while    
}

void scMessagePack::splitParamAsArray(const scDataNode &paramForSplit, const scString &a_varName, scDataNode &outputParam)
{
  int childCount = const_cast<scDataNode &>(paramForSplit).getArray()->size();
  int partCount;
  if (childCount < getChunkCount())
    partCount = childCount;
  else
    partCount = getChunkCount();
    
  int partSize;

  if (partCount >= 1)
    partSize = int(float(childCount) / float(partCount));  
  else
    partSize = 1;
      
  if (partSize < 1)
    partSize = 1;
    
  std::auto_ptr<scDataNode> paramGuard;
  scDataNode *newPart;
  scDataNode node; 
  int addedCount = 0, currPartCount;
  
  while (addedCount < childCount) 
  {
    paramGuard.reset(new scDataNode());
    newPart = paramGuard.get();
    currPartCount = 0;
    while((currPartCount < partSize) && (addedCount + currPartCount < childCount))
    {
      const_cast<scDataNode &>(paramForSplit).getArray()->getItem(
        addedCount + currPartCount, 
        node
      );
      newPart->addItem(node);    
      ++currPartCount;
    }
    outputParam.addChild(a_varName, paramGuard.release());
    addedCount += currPartCount;
  } // while    
}

/// Range is a record with two values: from, to
/// Each value is a record of: int / float + border_type(closed, open, wide_open)
/// Border types:
/// - closed:    include the specified value in the range
/// - open:      do not include specified value in the range
/// - wide_open: +/-inf depending on border value
/// - <null>:    means inf (sign depends on field - "from": -inf, "to": +inf)
/// Example:
///   (-3:closed, +5:wide_open) means two ranges: <-3, +5> and (+5, +inf)
void scMessagePack::splitParamAsRange(const scDataNode &paramForSplit, const scString &a_varName, scDataNode &outputParam)
{
  // TODO: implement "splitParamAsRange"
  throw scError("todo:To be implemented");
}
