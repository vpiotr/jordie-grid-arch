/////////////////////////////////////////////////////////////////////////////
// Name:        NodeRegistry.h
// Project:     grdLib
// Purpose:     Address book for processing nodes.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/NodeRegistry.h"
#include "perf/time_utils.h"
#include "base/wildcard.h"
#include "sc/utils.h"

// ----------------------------------------------------------------------------
// scRegistryEntry
// ----------------------------------------------------------------------------
scRegistryEntry::scRegistryEntry(const scString &address, scRegEntryFeatureMask features)
{
  m_address = address;
  m_features = features;
  m_shareTime = 0;
  m_endTime = 0;
}

scRegistryEntry::~scRegistryEntry()
{
}

void scRegistryEntry::setAddress(const scString &name)
{
  m_address = name;
}

void scRegistryEntry::getAddress(scString &output) const
{
  output = m_address;
}

const scString &scRegistryEntry::getAddress() const
{
  return m_address;
}

void scRegistryEntry::setFeatures(scRegEntryFeatureMask features)
{
  m_features = features;
}

scRegEntryFeatureMask scRegistryEntry::getFeatures() const
{
  return m_features;
}

void scRegistryEntry::setShareTime(cpu_ticks value)
{
  m_shareTime = value;
}  

cpu_ticks scRegistryEntry::getShareTime()
{
  return m_shareTime;
}

void scRegistryEntry::setEndTime(cpu_ticks value)
{
  m_endTime = value;
}

bool scRegistryEntry::isValid()
{
  if (m_endTime > 0)
    return (m_endTime > cpu_time_ms());
  else
    return true;  
}

void scRegistryEntry::addService(const scString &name)
{
  m_services.push_back(name);
}

void scRegistryEntry::removeService(const scString &name)
{
  scStringList::iterator it;
  it = find(m_services.begin(), m_services.end(), name);
  if (it != m_services.end())
    m_services.erase(it);
}

bool scRegistryEntry::matchesService(const scString &name) const
{
  bool res = false;
  scStringList::const_iterator it = m_services.begin();
  while(it != m_services.end())
  {
    if(wildcardMatch(*it, name))
    {
      res = true;
      break;
    }
    ++it;
  }  
  return res;
}


// ----------------------------------------------------------------------------
// scNodeRegistry
// ----------------------------------------------------------------------------
scNodeRegistry::scNodeRegistry()
{
  m_registrationNo = 0;
  m_nextHandle = 1;
}

scNodeRegistryHandle scNodeRegistry::genNextHandle()
{
  ulong64 res = m_nextHandle;
  m_nextHandle++;
  return res;
}

scNodeRegistryHandle scNodeRegistry::registerNodeForRole(const scMessageAddress &address, const scString &a_role, 
  scRegEntryFeatureMask features)
{
  scNodeRegistryHandle res = genNextHandle();

  scString roleName;
  if (a_role.substr(0, 1) == "@")
    roleName = a_role.substr(1);
  else
    roleName = a_role;  
    
  scRegistryEntryGuard guard(
      new scRegistryEntry(
        address.getAsString(),
        features
  ));
    
  m_handleMap.insert(std::pair<scNodeRegistryHandle, scRegistryEntryGuard>(res, guard));  
  m_roleRegistry.insert(std::pair<scString, scRegistryEntryGuard>(roleName, guard));
    
  return res;   
}

scNodeRegistryHandle scNodeRegistry::registerNodeForPath(const scMessageAddress &address, const scString &path,
  scRegEntryFeatureMask features)
{
  scNodeRegistryHandle res = genNextHandle();
  scRegistryEntryGuard guard(
      new scRegistryEntry(
        address.getAsString(),
        features
  ));
    
  m_handleMap.insert(std::pair<scNodeRegistryHandle, scRegistryEntryGuard>(res, guard));  
  m_exactRegistry.insert(std::pair<scString, scRegistryEntryGuard>(path, guard));
  
  return res;
}

scNodeRegistryHandle scNodeRegistry::registerNodeForName(const scMessageAddress &address, const scString &name,
  scRegEntryFeatureMask features)
{
  scNodeRegistryHandle res = genNextHandle();
  scRegistryEntryGuard guard(
      new scRegistryEntry(
        address.getAsString(),
        features
  ));

  m_handleMap.insert(std::pair<scNodeRegistryHandle, scRegistryEntryGuard>(res, guard));  
  m_exactRegistry.insert(std::pair<scString, scRegistryEntryGuard>(name, guard));
    
  return res;
}

void scNodeRegistry::registerNodeService(const scString &sourceKey, const scString &serviceName)
{
  scMessageAddress addr;
  addr.set(sourceKey);
  switch (addr.getFormat()) {
    case scMessageAddress::AdrFmtRole:  
      registerNodeServiceForRole(addr.getRole(), serviceName);
      break;
    default:
      registerNodeServiceForAddress(sourceKey, serviceName);          
  }
}

void scNodeRegistry::registerNodeServiceForRole(const scString &sourceKey, const scString &serviceName)
{
  scRoleRegistryMMapIterator fi;  
  fi = m_roleRegistry.find(sourceKey);
  if(fi != m_roleRegistry.end())
  {
    fi->second->addService(serviceName);
  }
}

void scNodeRegistry::registerNodeServiceForAddress(const scString &sourceKey, const scString &serviceName)
{
  scNameRegistryMapIterator fi;
  fi = m_exactRegistry.find(sourceKey);
  if(fi != m_exactRegistry.end())
  {
    fi->second->addService(serviceName);
  }
}


const scStringList scNodeRegistry::getAddrList(const scString &address, bool &unknownAlias)
{
  scStringList res;

  unknownAlias = false;
  scMessageAddress addr;
  addr.set(address);
  
  switch (addr.getFormat()) 
  {
    case scMessageAddress::AdrFmtVPath:
      res = getAddrListForPath(address);
      break;
    case scMessageAddress::AdrFmtRole:  
      res = getAddrListForRole(addr.getRole());
      break;
    default:
      if (!addr.getNode().empty())
        res = getAddrListForName(addr.getNode());
      else   
        res = getAddrListForName(address);
      
      if (res.empty()) {
        if (addr.getFormat() == scMessageAddress::AdrFmtRaw) {
          unknownAlias = true;
        } else {
          res.push_back(address);
        }  
      }         
  }  
  return res;  
}

bool scNodeRegistry::isRegistered(const scString &address)
{
  bool uknownAddrFormat;
  scStringList addrList = getAddrList(address, uknownAddrFormat);
  return (!addrList.empty());
}

scStringList scNodeRegistry::getAddrListForRole(const scString &role)
{
  scStringVector res;
  scRoleRegistryMMapIterator fi;
  
  fi = m_roleRegistry.find(role);
  if(fi != m_roleRegistry.end()) { // found a name
    do {
      res.push_back(fi->second->getAddress());
      ++fi;
    } while (fi != m_roleRegistry.upper_bound(role));
  }

  // remove duplicates  
  std::sort( res.begin(), res.end());
  res.erase(std::unique(res.begin(), res.end()), res.end());
  
  scStringList resList;
  std::copy(res.begin(), res.end(), back_inserter(resList));
  return resList;
}

scStringList scNodeRegistry::getAddrListForPath(const scString &path)
{
  scStringList res;
  scNameRegistryMapIterator fi;
  fi = m_exactRegistry.find(path);
  if(fi != m_exactRegistry.end())
  {
    res.push_back(fi->second->getAddress());
  }
  
  return res;
}

scStringList scNodeRegistry::getAddrListForName(const scString &name)
{
  return getAddrListForPath(name);
}

void scNodeRegistry::getAddrListForRole(const scString &role, bool publicOnly, scStringList &output) const
{
  getAddrListForRoleAndKey(role, scString(), publicOnly, output);
}

void scNodeRegistry::getAddrListForRoleAndKey(const scString &role, const scString &searchKey, 
  bool publicOnly, scStringList &output) const
{
  scRoleRegistryMMapCIterator fi;

  output.clear();  
  fi = m_roleRegistry.find(role);
  if(fi != m_roleRegistry.end()) { // found a name
    do {
      if (!publicOnly || (fi->second->getFeatures() & refPublic) != 0)
      {
        if (searchKey.empty() || fi->second->matchesService(searchKey))
        {
          if ((fi->second->getFeatures() & refDirectMode) != 0)
            output.push_back(fi->second->getAddress());
          else
            output.push_back(role);  
        }    
      }  
      ++fi;
    } while (fi != m_roleRegistry.upper_bound(role));
  }
}  

void scNodeRegistry::getAddrListForRoleAndKey(const scString &role, const scString &searchKey, 
  bool publicOnly, scDataNode &output) const
{
  scRoleRegistryMMapCIterator fi;
  std::auto_ptr<scDataNode> entryGuard;

  output.clear();  
  fi = m_roleRegistry.find(role);
  if(fi != m_roleRegistry.end()) { // found a name
    do {
      if (!publicOnly || (fi->second->getFeatures() & refPublic) != 0)
      {
        if (searchKey.empty() || fi->second->matchesService(searchKey))
        {
          entryGuard.reset(new scDataNode());
          if ((fi->second->getFeatures() & refDirectMode) != 0)
            entryGuard->addChild("address", new scDataNode(fi->second->getAddress()));
          else
            entryGuard->addChild("address", new scDataNode(role));

          entryGuard->addChild("share_time", new scDataNode(fi->second->getShareTime()));
          output.addChild(entryGuard.release());
        }    
      }  
      ++fi;
    } while (fi != m_roleRegistry.upper_bound(role));
  }
}  

scString scNodeRegistry::genRegistrationId(const scString &address)
{
  m_registrationNo++;
  return genGuid(m_registrationNo);  
}

scRegistryEntry *scNodeRegistry::findEntry(scNodeRegistryHandle entryHandle)
{
  scNodeRegistryHandleMap::iterator it = m_handleMap.find(entryHandle);
  if (it != m_handleMap.end())
  {
    return it->second.get();    
  } else 
  {
    return SC_NULL;
  }  
}

void scNodeRegistry::setEntryShareTime(scNodeRegistryHandle entryHandle, cpu_ticks shareTime)
{
  scRegistryEntry *entry = findEntry(entryHandle);
  if (entry == SC_NULL)
    throw scError(scString("Unknown handle: ")+toString(entryHandle));
  entry->setShareTime(shareTime);  
}

void scNodeRegistry::setEntryEndTime(scNodeRegistryHandle entryHandle, cpu_ticks value)
{
  scRegistryEntry *entry = findEntry(entryHandle);
  if (entry == SC_NULL)
    throw scError(scString("Unknown handle: ")+toString(entryHandle));
  entry->setEndTime(value);  
}

void scNodeRegistry::validateEntries()
{
  scNodeRegistryHandleMap::iterator it = m_handleMap.begin();

  while(it != m_handleMap.end())
  {
    if (!it->second->isValid())
      it = m_handleMap.erase(it);
    else
      ++it;  
  }
  
  scRoleRegistryMMap::iterator roleIt = m_roleRegistry.begin();
  while(roleIt != m_roleRegistry.end())
  {
    if (!roleIt->second->isValid())
      roleIt = m_roleRegistry.erase(roleIt);
    else
      ++roleIt;  
  }  

  scNameRegistryMapIterator nameIt = m_exactRegistry.begin();
  while(nameIt != m_exactRegistry.end())
  {
    if (!nameIt->second->isValid())
      nameIt = m_exactRegistry.erase(nameIt);
    else
      ++nameIt;  
  }  
}
