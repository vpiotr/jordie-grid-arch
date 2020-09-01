/////////////////////////////////////////////////////////////////////////////
// Name:        SmplQueue.h
// Project:     grdLib
// Purpose:     Simple queue handling. Can be transparent for client 
//              and worker nodes. 
// Author:      Piotr Likus
// Modified by:
// Created:     15/10/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _SMPLQUEUE_H__
#define _SMPLQUEUE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file SmplQueue.h
///

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "grd/core.h"
#include "grd/TaskImpl.h"
#include "grd/RequestItem.h"
#include "grd/ModuleImpl.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
enum scSmplQueueType {
  sstNullDev,
  sstRoundRobin,
  sstPull,
  sstMultiCast,
  sstForward,
  sstHighAvail
};

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class scSmplQueueModule;
class scSmplQueueManagerTask;
class scSmplQueueReaderTask;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const scString GRD_SQUEUE_TYPE_ROUND_ROBIN = "rrobin";
const scString GRD_SQUEUE_TYPE_PULL        = "pull";
const scString GRD_SQUEUE_TYPE_MULTICAST   = "mcast";
const scString GRD_SQUEUE_TYPE_NULL_DEV    = "null_dev";
const scString GRD_SQUEUE_TYPE_FORWARD     = "forward";
const scString GRD_SQUEUE_TYPE_HIGHAVAIL   = "highav";

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
typedef std::list<scTask *> scReaderList;
typedef scReaderList::iterator scReaderListIterator;
//typedef boost::ptr_list<scSmplQueueManagerTask> scSmplQueueManagerColn;
typedef std::list<scSmplQueueManagerTask *> scSmplQueueManagerList;
typedef std::auto_ptr<scTask> scSmplTaskGuard;

// ----------------------------------------------------------------------------
// scSmplQueueManagerTask
// ----------------------------------------------------------------------------
/// Task for queue management. Exists with owned queue, one per queue.
/// Receives messages using standard MPI and stores them for future use.
class scSmplQueueManagerTask: public scTask {
public:
  // create
  scSmplQueueManagerTask(bool allowSenderAsReader);
  virtual ~scSmplQueueManagerTask();
  // properties
  void setLimit(int value);
  int getLimit() const;
  // run
  virtual int handleMessage(scEnvelope &envelope, scResponse &response);
  //virtual int handleMessage(scMessage *message, scResponse &response);    
  virtual void addReader(scSmplQueueReaderTask *reader);
  void removeReader(scSmplQueueReaderTask *reader);
  virtual void deleteReaders();
  virtual bool get(scEnvelope &a_envelope);
  virtual void put(const scEnvelope &envelope);  
  void clearQueue();
  bool isEmpty() const;
  virtual scString getStatus();
  virtual bool needsRun();
  void getReaderList(scStringList &list);
  virtual bool hasMessageForReader(scSmplQueueReaderTask *reader);
  bool hasReader(const scString &readerTarget);
  bool getAllowSenderAsReader() {return m_allowSenderAsReader;}
  virtual bool handleReaderResponse(
    scSmplQueueReaderTask &reader, const scString &readerTarget, 
    const scEnvelope &envelope, const scResponse &response
  );
  virtual bool markReaderAlive(const scString &readerAddr);
  virtual void handleEnvelopeAccepted(scSmplQueueReaderTask *reader, const scEnvelope &envelope);
  virtual void handleEnvelopeSent(scSmplQueueReaderTask *reader, const scEnvelope &envelope);
protected:  
  scReaderListIterator findReader(const scString &name);
  scString findNextReaderName(const scString &readerName);
  void disconnectReaders();
protected:  
  int m_limit;
  scEnvelopeColn m_waiting;
  scReaderList m_readers;
private:
  //scString m_lastReaderName;
  bool m_allowSenderAsReader;
};

// ----------------------------------------------------------------------------
// scSmplQueueReaderTask
// ----------------------------------------------------------------------------
/// Task used scan input queue and forward message to assigned node
class scSmplQueueReaderTask: public scTask {
public:
  //--- construction ---  
  scSmplQueueReaderTask();
  virtual ~scSmplQueueReaderTask();
  // properties
  void setQueueManager(scSmplQueueManagerTask *queueManager);
  scSmplQueueManagerTask *getQueueManager();
  void setTarget(const scString &target);
  scString getTarget() const;
  void setLimit(int value);
  int getLimit() const;
  //--- task intf ---
  virtual int handleResponse(scMessage *message, scResponse &response); ///< handle processed messages
  virtual int intRun(); ///< check if read can be performed
  virtual bool needsRun();
  virtual int runStopping(); 
  //--- other ---   
  void noteContactEvent();
  cpu_ticks getLastContactTime();
  bool forwardEnvelope(scEnvelope &envelope);
  bool acceptEnvelope(const scEnvelope &envelope);
  void sendResponse(const scEnvelope &envelope, const scResponse &response);  
  void cancelRequest(uint requestId, int reasonCode);
  void cancelAll();
protected:
  void addWaitingMsg(scEnvelope &envelope, int requestId);
  bool extractWaitingMsg(int requestId, scRequestItem &foundItem);
  bool isBelowLimit();
  int handleResponse(uint requestId, scResponse &response);
  bool isMessageReadyForRead();
private:
  scString m_target; ///< target node
  int m_processed; ///< number of processed messages
  int m_limit; ///< limit for messages handled in parallel, 0 - no limit
  scRequestItemMapColn m_waitingRequests; ///< messages sent and waiting to be answered
  scSmplQueueManagerTask *m_queueManager;
  bool m_allowSenderAsReader;
  cpu_ticks m_lastContactTime;
};

// ----------------------------------------------------------------------------
// scSmplQueueModule
// ----------------------------------------------------------------------------
/// Module responding to initial queue messages
/// Handled commands:
/// - squeue.init (qname[,limit]) - returns address of initiated queue (responsible task)
///                               (creates scSmplQueueManagerTask)
/// - squeue.listen (qname, target) - creates a task that will forward message to target,
///                                 when message is handled, this tasks receives answer and forwards it
///                                 (scSmplQueueReaderTask)
/// - squeue.close (qname) - close queue
/// - squeue.list_readers (qname) - list assigned readers
/// - squeue.clear (qname) - empty queue
/// - squeue.get_status (qname) - returns number of msgs in queue, number of readers
/// - squeue.mark_alive(exec_at_addr, queue_name, source_name) - mark sender (source_name) as alive in queue
/// - squeue.keep_alive(address, queue_name, msg_limit, delay, error_limit)
///
/// Note: both reader and manager should be running on the same node.
class scSmplQueueModule: public scModule {
public:
  // -- creation --
  scSmplQueueModule();
  virtual ~scSmplQueueModule();
  void clearTask(scTask *task);
  // -- module support --
  virtual scStringList supportedInterfaces() const;
  virtual int handleMessage(scMessage *message, scResponse &response);
  virtual scTaskIntf *prepareTaskForMessage(scMessage *message);
  // --- commands ---
  scTask *prepareManager(scMessage *message);
  scTask *prepareReader(scMessage *message);
protected:
  // --- commands ---
  int handleCmdInit(scMessage *message, scResponse &response);
  int handleCmdListen(scMessage *message, scResponse &response);
  int handleCmdListenAt(scMessage *message, scResponse &response);
  int handleCmdClose(scMessage *message, scResponse &response);
  int handleCmdClear(scMessage *message, scResponse &response);
  int handleCmdGetStatus(scMessage *message, scResponse &response);   
  int handleCmdListReaders(scMessage *message, scResponse &response);
  int handleCmdMarkAlive(scMessage *message, scResponse &response);
  int handleCmdKeepAlive(scMessage *message, scResponse &response);
  // --- other ---  
  bool queueExists(const scString &name);
  scSmplQueueManagerTask *createQueue(scSmplQueueType qtype, const scString &name, 
    bool allowSenderAsReader, const scString &forwardToAddr, bool durable, const scDataNode &extraParams);
  scSmplQueueManagerTask *findQueue(const scString &name);
  scSmplQueueManagerTask *checkQueue(const scString &name);
  void closeQueue(const scString &name);
  void setUnknownQueueError(const scString &qname, scResponse &response);
  scTask *prepareReader(scSmplQueueManagerTask *queue, const scString &target);
  bool performMarkAlive(const scString &queueName, const scString &srcName);
  scTask *prepareKeepAliveTask(scMessage *message);
  scTask *createKeepAliveTask();
  scTask *getKeepAliveTask();
private:
  scSmplQueueManagerList m_managers;
  scSmplTaskGuard m_aliveNotifier;
  scTask *m_keepAliveTask;
};

#endif // _SMPLQUEUE_H__