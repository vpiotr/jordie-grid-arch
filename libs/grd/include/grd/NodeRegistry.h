/////////////////////////////////////////////////////////////////////////////
// Name:        NodeRegistry.h
// Project:     grdLib
// Purpose:     Address book for processing nodes.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _GRDNODEREG_H__
#define _GRDNODEREG_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/** \file NodeRegistry.h
\brief Address book for processing nodes.

Long description
*/

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/MessageAddress.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------
enum scRegEntryFeature { 
  refPublic = 1, 
  refDirectMode = 2
};

typedef uint scRegEntryFeatureMask;
class scRegistryEntry;
typedef boost::shared_ptr<scRegistryEntry> scRegistryEntryGuard;
typedef std::multimap<scString,scRegistryEntryGuard> scRoleRegistryMMap;
typedef scRoleRegistryMMap::iterator scRoleRegistryMMapIterator; 
typedef scRoleRegistryMMap::const_iterator scRoleRegistryMMapCIterator; 

typedef std::map<scString,scRegistryEntryGuard> scNameRegistryMap;
typedef scNameRegistryMap::iterator scNameRegistryMapIterator;

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
// scNodeRegistry
// ----------------------------------------------------------------------------
/// registry for nodes
typedef ulong64 scNodeRegistryHandle;
typedef std::map<scNodeRegistryHandle,scRegistryEntryGuard> scNodeRegistryHandleMap;
 
class scNodeRegistry 
{
public:
    scNodeRegistry();
    virtual ~scNodeRegistry(){};
    /// node will be accessible using 'node-name' in node name part, name needs to be alphanumeric
    scNodeRegistryHandle registerNodeForName(const scMessageAddress &address, const scString &name, scRegEntryFeatureMask features = 0);   
    /// node will be accessible using '@role'
    scNodeRegistryHandle registerNodeForRole(const scMessageAddress &address, const scString &a_role, scRegEntryFeatureMask features = 0);   
    /// node will be accessible using '//address/part1/partN'
    scNodeRegistryHandle registerNodeForPath(const scMessageAddress &address, const scString &path, scRegEntryFeatureMask features = 0);   
    /// register service available under a given source name 
    void registerNodeService(const scString &sourceKey, const scString &serviceName);
    void unregisterNode(const scMessageAddress &address, const scString &name, const scString &role = scString(""));   
    /// returns list of full addresses for partially defined address
    scStringList getAddrList(const scMessageAddress &address);
    // verifies if there is any mapping for a given address
    bool isRegistered(const scString &address);
    /// returns list of full addresses for partially defined address
    const scStringList getAddrList(const scString &address, bool &unknownAlias);
    /// returns list of full addresses for specified name (should be 1)
    scStringList getAddrListForName(const scString &name);
    /// returns list of full addresses for specified role name 
    scStringList getAddrListForRole(const scString &role);
    /// returns list of full addresses for specified path
    scStringList getAddrListForPath(const scString &path);
    /// returns list of full addresses for specified role name 
    void getAddrListForRole(const scString &role, bool publicOnly, scStringList &output) const;
    /// returns list of full addresses for specified role name and service key
    void getAddrListForRoleAndKey(const scString &role, const scString &searchKey, 
      bool publicOnly, scStringList &output) const;
    void getAddrListForRoleAndKey(const scString &role, const scString &searchKey, 
      bool publicOnly, scDataNode &output) const;
    scString genRegistrationId(const scString &address);          
    void setEntryShareTime(scNodeRegistryHandle entryHandle, cpu_ticks shareTime);
    void setEntryEndTime(scNodeRegistryHandle entryHandle, cpu_ticks value);  
    void validateEntries();  
protected:      
    void registerNodeServiceForRole(const scString &sourceKey, const scString &serviceName);
    void registerNodeServiceForAddress(const scString &sourceKey, const scString &serviceName);
    scNodeRegistryHandle genNextHandle();
    scRegistryEntry *findEntry(scNodeRegistryHandle entryHandle);
protected:
    uint m_registrationNo;
    scNodeRegistryHandle m_nextHandle; 
    scRoleRegistryMMap m_roleRegistry;        
    scNameRegistryMap m_exactRegistry;
    scNodeRegistryHandleMap m_handleMap;
};

class scRegistryEntry {
public:  
  // create
  scRegistryEntry(const scString &address, scRegEntryFeatureMask features);
  ~scRegistryEntry();
  // properties
  void setAddress(const scString &name);
  void getAddress(scString &output) const;
  const scString &getAddress() const;
  void setFeatures(scRegEntryFeatureMask features);
  scRegEntryFeatureMask getFeatures() const;
  void setShareTime(cpu_ticks value);
  void setEndTime(cpu_ticks value);
  cpu_ticks getShareTime();
  bool isValid();
  // run
  void addService(const scString &name);
  void removeService(const scString &name);
  bool matchesService(const scString &name) const;  
protected:
  scRegEntryFeatureMask m_features;
  cpu_ticks m_shareTime;
  cpu_ticks m_endTime;
  scString m_address;
  scStringList m_services;    
};


#endif // _GRDNODEREG_H__