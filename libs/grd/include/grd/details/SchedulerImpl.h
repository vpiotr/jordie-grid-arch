/////////////////////////////////////////////////////////////////////////////
// Name:        SchedulerImpl.h
// Project:     grdLib
// Purpose:     Scheduler interface implementation.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDSCHEDIMPL_H__
#define _GRDSCHEDIMPL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file SchedulerImpl.h
\brief Scheduler interface implementation.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// std
#include <vector>

// sc
#include "sc/dtypes.h"

// grd
#include "grd/Scheduler.h"
#include "grd/Module.h"
#include "grd/Task.h"
#include "grd/MessageGate.h"
#include "grd/NodeRegistry.h"
#include "grd/Message.h"
#include "grd/Response.h"
#include "grd/LocalNodeRegistry.h"
#include "grd/RequestHandler.h"
#include "grd/RequestItem.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// scModuleList
// ----------------------------------------------------------------------------
/// List of registered message handlers
typedef std::vector<scModuleIntf *> scModuleList;
typedef std::vector<scModuleIntf *>::iterator scModuleListIterator;

// ----------------------------------------------------------------------------
// scTaskColn
// ----------------------------------------------------------------------------
/// Owning collection of tasks
typedef boost::ptr_list<scTaskIntf> scTaskColn;
typedef boost::ptr_list<scTaskIntf>::iterator scTaskColnIterator;

// ----------------------------------------------------------------------------
// scMessageGateColn
// ----------------------------------------------------------------------------
/// Owning collection of message gates
typedef boost::ptr_list<scMessageGate> scMessageGateColn;
typedef boost::ptr_list<scMessageGate>::iterator scMessageGateColnIterator;

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
// scCommandMapIntf
// ----------------------------------------------------------------------------
class scCommandMapIntf {
public:
  scCommandMapIntf() {}
  virtual ~scCommandMapIntf() {}

  virtual void registerCommandMap(const scString &cmdFilter, const scString &targetName, int priority) = 0;
  virtual bool findTargetForCommand(const scString &command, scString &target) = 0;
};

// ----------------------------------------------------------------------------
// scScheduler
// ----------------------------------------------------------------------------
/// A process manager (scheduler)
class scScheduler: public scSchedulerIntf
{
public:
    scScheduler();
    virtual ~scScheduler();
    static scScheduler *newScheduler();
    virtual void init();

    // --- interface - general functions
    virtual void addTask(scTaskIntf *a_task);
    virtual void deleteTask(scTaskIntf *a_task);

    // --- interface - address functions
    virtual scMessageAddress getOwnAddress(const scString &protocol = scString(""));
    virtual scString evaluateAddress(const scString &virtualAddr);

    // --- interface - posting functions

    /// send message through dispatcher or this scheduler (if dispatcher not found)
    virtual bool forwardMessage(const scString &address, const scString &command, 
      const scDataNode *params, int requestId, scRequestHandler *handler);
    virtual scEnvelope *createErrorResponseFor(const scEnvelope &srcEnvelope, const scString &msg, int a_status);
    virtual void postEnvelopeForThis(scEnvelope *envelope);
    virtual void postEnvelope(scEnvelope *envelope, scRequestHandler *handler = SC_NULL);    

    // --- architecture ---     
    virtual void addModule(scModuleIntf *a_handler);

    virtual void addInputGate(scMessageGate *a_gate);
    virtual void addOutputGate(scMessageGate *a_gate);

    virtual scTaskIntf *extractTask(scTaskIntf *a_task);
    virtual bool taskExists(const scString &a_name);
    virtual uint getNonDeamonTaskCount();
    
    virtual scTaskIntf *findTaskForMessage(const scString &command, const scDataNode &params);
    
    virtual bool registerNodeAs(const scString &source, const scString &target, scString &newName, 
      bool publicEntry = false, bool directMode = false, cpu_ticks shareTime = 0, cpu_ticks endTime = 0);
    virtual bool hasNodeInRegistry(const scString &source);  
      
    virtual void registerNodeService(const scString &sourceKey, const scString &serviceName);
    virtual void getRegistryEntriesForRole(const scString &protocol, const scString &roleName, const scString &searchKey, 
      bool publicOnly, scDataNode &output);

    virtual void registerCommandMap(const scString &cmdFilter, const scString &targetName, int priority);
    
    void createNodes(const scString &a_className, int nodeCount, const scString &a_namePattern); 
    void setDispatcher(const scString &address);
    void setDirectoryAddr(const scString &address);
    scString getDirectoryAddr();
    bool isDirectoryNull();
    // --- processing ---     
    virtual void postMessage(const scString &address, const scString &command, 
      const scDataNode *params = SC_NULL, 
      int requestId = SC_REQUEST_ID_NULL,
      scRequestHandler *handler = SC_NULL);
    virtual bool cancelRequest(int requestId);
    /// process all waiting messages
    virtual void flushEvents();
    virtual void run();
    virtual bool needsRun();
    void getStats(int &taskCnt, int &moduleCnt, int &gateCnt);    
    virtual int getNextRequestId();
    virtual void requestStop();
    int dispatchMessage(const scMessage &message, scResponse &response);
    // --- properties --- 
    virtual void setName(const scString &a_name);
    virtual scString getName() const;  
    uint getFeatures();
    void setFeatures(uint value);
    bool isFeatureActive(scSchedulerFeature feature);
    //---
    scString getRegistrationId() const;  
    void setRegistrationId(const scString &value);
    void setLocalRegistry(scLocalNodeRegistry *registry);
    virtual scSchedulerStatus getStatus() const;
protected:
    void checkCleanup();
    void performCleanup();
    void registerSelf();
    unsigned int getNextTaskId(); 
    /// find output gate for protocol. empty = first, not found = exception
    scMessageGate &findOutGateForProtocol(const scString &protocol);      
    scMessageGate &findInpGateForThis();
    void postMessageForAddress(const scString &address, const scString &command, 
      const scDataNode *params, 
      int requestId,
      scRequestHandlerTransporter &transporter);    
    void postEnvelopeForAddress(const scString &address,
      scEnvelope *envelope, scRequestHandler *handler);
    void runGates();
    void runMessages();
    void runTasks();
    int intDispatchMessage(const scEnvelope &envelope, scResponse *a_response = SC_NULL);
    int dispatchMessageForTasks(const scEnvelope &envelope, scResponse *a_response);
    int dispatchMessageForOneTask(const scEnvelope &envelope, scResponse *a_response);
//    int dispatchMessageForAnyTask(scEnvelope *envelope);
    int dispatchMessageForModules(const scEnvelope &envelope, scResponse *a_response);
    bool checkPostResponse(const scEnvelope &orgEnvelope, const scResponse &response);    
    void postResponse(const scEnvelope &orgEnvelope, const scResponse &response);
    void handleResponse(const scEnvelope &envelope);
    void throwDispatchError(int status, const scEnvelope &envelope);
    bool matchResponse(int requestId, scRequestItem &foundItem);
    bool matchResponse(const scEnvelope &envelopeResponse, scRequestItem &foundItem);
    scTaskIntf *findTask(const scMessageAddress &address);
    int dispatchResponseForHandlers(scEvent *eventMessage, scEvent *eventResponse);
    bool isOwnAddress(const scMessageAddress &value);     
    bool isOwnAddressSkipTask(const scMessageAddress &value);
    scStringList getAddrList(const scString &address, bool &unknownAlias);
    bool isIndirectAddress(const scString &address);
    scString resolveTaskName(const scString &address);
    virtual void prepareDefaultGates();
    int dispatchMessageForModulesDirect(const scEnvelope &envelope, scResponse *a_response);    
    int dispatchMessageForModulesByIntf(const scEnvelope &envelope, scResponse *a_response);    
    int handleMessageByModule(scModuleIntf &handler, const scEnvelope &envelope, scResponse &response, bool postResponse);
    void handleUnknownResponse(const scEnvelope &envelope);
    void handleResponseByReqHandler(const scEnvelope &orgEnvelope,
      const scEnvelope &respEnvelope, scRequestHandler *handler);
    void handleDispatchError(int status, const scEnvelope &envelope);    
    scTaskColnIterator findTask(const scString &name);
    scString genNewNodeName(const scString &a_coreName);
    void checkClose();
    void setStatus(scSchedulerStatus value);
    void checkTimeouts();
    bool gatesEmpty(); 
    bool tasksNeedsRun();
    bool forwardEnvelope(scEnvelope *envelope, scRequestHandler *handler);
    void notifyHandlersTaskDelete(scTaskIntf *a_task);
    virtual bool resolveDestForMessage(const scString &address, const scString &command, 
          const scDataNode *params, int requestId, scRequestHandler *handler);
    void handleResolveFailed(const scString &address, const scString &command, 
          const scDataNode *params, int requestId, scRequestHandler *handler);
    bool isSameProtocol(const scString &protocol1, const scString &protocol2);
    void registerNodeAtDirectory(const scString &srcAddr, const scString &targetAddr, cpu_ticks shareTime = 0);
protected:    
    //void notifyObserversMsgArrived(const scMessage &message);
    void notifyObserversEnvArrived(const scEnvelope &envelope);
    void notifyObserversMsgHandled(const scMessage &message, const scResponse &response, int resultStatus);
    void notifyObserversMsgHandleError(const scMessage &message, const scResponse &response, const std::exception& excp);
    void notifyObserversMsgReadyForSend(const scEnvelope &envelope, const scMessageGate &gate);
    void notifyObserversMsgWaitStarted(const scEnvelope &envelope, uint requestId);
    void notifyObserversMsgWaitEnd(uint requestId);
    void notifyObserversResponseArrived(const scEnvelope &envelope, const scRequestItem &reqItem);
    void notifyObserversResponseUnknownIdArrived(const scEnvelope &envelope);
    void notifyObserversResponseHandled(const scEnvelope &envelope, const scRequestItem &reqItem);
    void notifyObserversResponseHandleError(const scEnvelope &envelope, const scRequestItem &reqItem, const scError &excp);
    void notifyObserversResponseHandleError(const scEnvelope &envelope, const scRequestItem &reqItem);
    scEnvelopeSerializerBase *checkEnvelopeSerializer();
protected:    
    scString m_directoryAddr;
    scString m_registrationId;
private:
    uint m_features;
    scSchedulerStatus m_status;
    scString m_name;
    scMessageGateColn m_inputGates;      
    scMessageGateColn m_outputGates;      
    scMessageGate *m_defInputGate;
    scMessageGate *m_defOutputGate;
    scTaskColn m_tasks;      
    scRequestItemMapColn m_waitingMessages; ///< messages waiting to be answered  
    scNodeRegistry m_registry;
    scModuleList m_modules;
    scLocalNodeRegistry *m_localRegistry;
    unsigned int m_nextTaskId;
    int m_nextRequestId;
    friend class scMessageGate;
    scString m_dispatcher;
    boost::shared_ptr<scEnvelopeSerializerBase> m_envelopeSerializer;
    cpu_ticks m_lastCleanupTime;
    std::auto_ptr<scCommandMapIntf> m_commandMap;
};


#endif // _GRDSCHEDIMPL_H__