/////////////////////////////////////////////////////////////////////////////
// Name:        JobWorkFile.h
// Project:     scLib
// Purpose:     Work file management
// Author:      Piotr Likus
// Modified by:
// Created:     23/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "wx/filename.h"

#include "sc/utils.h"

#include "grd/JobWorkFile.h"
#include "grd/JobCommon.h"

// ----------------------------------------------------------------------------
// scJobWorkFile
// ----------------------------------------------------------------------------
scJobWorkFile::scJobWorkFile()
{
  m_previousName = "";
  m_currentName = "";  
  m_seqNo = 0;
  m_namePattern = "";  
  m_obsolNamePfx = m_tempNamePfx = m_workNamePfx = ""; 
}

scJobWorkFile::~scJobWorkFile()
{
}

void scJobWorkFile::init(uint seqNo, const scString &namePattern, const scString &regCoreName)
{
  m_seqNo = seqNo;
  m_namePattern = namePattern;  
  
  m_coreName = regCoreName;  
  m_tempNamePfx = scString("temp_")+regCoreName;
  m_workNamePfx = scString("work_")+regCoreName;
  m_obsolNamePfx = scString("obs_")+regCoreName;
  
  if (seqNo > 0)
    m_previousName = genFileName(m_namePattern, m_seqNo - 1);
  else  
    m_previousName = "";
  m_currentName = "";  
}

void scJobWorkFile::startWorkUnit(bool lastUnit)
{
  m_currentName = genFileName(m_namePattern, m_seqNo);

  // mark new file for deletion on rollback
  wxFileName filename(m_currentName);
  scString outFilePath = filename.GetFullPath();

  if (lastUnit) {
    addAlloc(m_tempNamePfx + toString(m_seqNo), JMM_RESTYP_TEMPFILE, outFilePath);        
  } else { 
    addAlloc(m_workNamePfx + toString(m_seqNo), JMM_RESTYP_WORKFILE, outFilePath);  
  }  
  if (m_seqNo > 0) {
    wxFileName oldFileName(m_previousName);
    scString oldFilePath = oldFileName.GetFullPath();    
    addAlloc(m_obsolNamePfx + toString(m_seqNo-1), JMM_RESTYP_OBSOLFILE, oldFilePath);    
  }    
}

void scJobWorkFile::commit()
{
  m_seqNo++;
  m_previousName = m_currentName;
  m_currentName = "";
}

scDataNode scJobWorkFile::extractPendingAllocs()
{
  scDataNode res;
  res.eatValueFrom(m_pendingAllocs);
  m_pendingAllocs.clear();
  return res;
}

scString scJobWorkFile::getCurrentName()
{
  return m_currentName;
}

scString scJobWorkFile::getPreviousName()
{
  return m_previousName;
}

scString scJobWorkFile::getCoreName()
{
  return m_coreName;
}  

scString scJobWorkFile::genFileName(const scString &nameTpl, uint seqNo)
{
  scDataNode params;
  params.addChild("seq_no", new scDataNode(seqNo));
  scString res = fillTemplateValues(nameTpl, params);
  return res;
}

uint scJobWorkFile::getSeqNo()
{
  return m_seqNo;
}

void scJobWorkFile::addAlloc(const scString &name, const scString &resType, const scString &path)
{
  std::auto_ptr<scDataNode> guard(new scDataNode());
  guard->addChild("name", new scDataNode(name));
  guard->addChild("type", new scDataNode(resType));
  guard->addChild("path", new scDataNode(path));
  m_pendingAllocs.addChild(guard.release());
}
