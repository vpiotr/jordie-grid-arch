/////////////////////////////////////////////////////////////////////////////
// Name:        core.h
// Project:     scLib
// Purpose:     Multi-node scheduler infrastructure. Core elements.
// Author:      Piotr Likus
// Modified by:
// Created:     23/09/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _CORE_H__
#define _CORE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
/// Library for work dispatching. 
/// Work can be performed by internal or external virtual processing nodes.
/// A node can be: internal object, a thread, a process or on another computer.
/// Virtual communication framework is provided.
///
/// Address format:
///   protocol::address
/// protocol:
///   inproc
///   tcp
///   ipc
///   pgm
/// address: 
///   #host/node/task
///   @role
///   virtual-path

/// Virtual message address:
///   Virtual address is handled by node registry and can be:
///   'simple-name' - simple unique ID with characters: 'a-zA-Z0-9\-\_' 
///   '@role-name'  - common role that can exist on each node
///   '\\path\to\node' - virtual, unique path to computer,node or task
///
/// Design patterns:
///   "collection" - structure owning objects
///   "list" - structure listing objects, ownership is somewhere else
///            can be implemented using list, queue, stack etc
///   "transporter" - smart pointer of selected type (shared_ptr, intrusive_ptr)
///   "guard" - object that holds ownership of pointer, function scope
/// 
/// Time slicing -> priority support (build-in for priority > 0, can be used in intRun):
/// 
/// int mytask::intRun()
/// {
///   startTimeslice();
///   do {
///    //... process job
///   } while (!isEndOfTimeslice());
///   return 1;
/// }
/// 
/// Message lifecycle:
/// gate->get
/// -> dispatch 
///    -> messageArrived
///    -> messageHandled
/// -> handleResponse              
///    -> responseArrived
///    -> notifyObserversMsgWaitEnd   
///    -> resposneHandling
///     
/// post
/// -> messageReadyForSend
/// -> gate->put                              
/// -> [] start waiting
///          
/// task->delete
/// -> notifyObserversMsgWaitEnd
///     
/// response-timeout
/// -> notifyObserversMsgWaitEnd

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
// #include ".."
// std
#include <stdexcept> // exceptions
// stl
#include <vector>
#include <map>
//#include <array>

// boost
#include <boost/shared_ptr.hpp>
#include "boost/ptr_container/ptr_list.hpp"
#include "boost/ptr_container/ptr_map.hpp"
#include <boost/any.hpp>
#include <boost/intrusive_ptr.hpp>

// dtp 
#include "base/serializer.h"

// local
#include "sc/dtypes.h"
#include "sc/utils.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------
class scEvent;
class scMessage;
class scResponse;
class scEnvelope;
class scTask;
class scScheduler;
class scModule;
class scMessageAddress;
class scMessageGate;
class scRequestItem;
class scRequestHandler;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
               
//const int SC_REQUEST_ID_DEFAULT     = 0;

const unsigned SC_DEF_PRIORITY = 5;

#ifdef SC_LOG_ENABLED
//#define SC_LOG_CORE
#endif

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
typedef std::map<scString,scString> scStringStringMap;
typedef scStringStringMap::iterator scStringStringMapIterator;

// ----------------------------------------------------------------------------
// scParamList
// ----------------------------------------------------------------------------
/// Named parameter list (variant)
typedef std::map<std::string, boost::any> scParamList;

//typedef std::queue<boost::shared_ptr<scEnvelope> > scEnvelopeQueueColn;
//typedef boost::shared_ptr<scEnvelope> scEnvelopeTransport;

// ----------------------------------------------------------------------------
// scSchedulerColn
// ----------------------------------------------------------------------------
/// Owning collection of schedulers
typedef boost::ptr_list<scScheduler> scSchedulerColn;
typedef boost::ptr_list<scScheduler>::iterator scSchedulerColnIterator;

// ----------------------------------------------------------------------------
// scModuleColn
// ----------------------------------------------------------------------------
/// Collection of owned message handlers
typedef boost::ptr_list<scModule> scModuleColn;
typedef boost::ptr_list<scModule>::iterator scModuleColnIterator;
typedef boost::ptr_map<scString, scModule> scModuleMap;

#endif // _CORE_H__