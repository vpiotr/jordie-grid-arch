/////////////////////////////////////////////////////////////////////////////
// Name:        JobWorkFile.h
// Project:     scLib
// Purpose:     Work file management
// Author:      Piotr Likus
// Modified by:
// Created:     23/01/2009
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _JOBWORKFILE_H__
#define _JOBWORKFILE_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file JobWorkFile.h
///
/// 

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"

// ----------------------------------------------------------------------------
// Simple type definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Forward class definitions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Class definitions
// ----------------------------------------------------------------------------
class scJobWorkFile {
public:
  scJobWorkFile();
  virtual ~scJobWorkFile();
  void init(uint seqNo, const scString &namePattern, const scString &regCoreName = scString("file"));
  void startWorkUnit(bool lastUnit);
  void commit();  
  scDataNode extractPendingAllocs();
  scString getCurrentName();
  scString getPreviousName();
  // getters, setters
  scString getCoreName();
  uint getSeqNo();
protected:
  void addAlloc(const scString &name, const scString &resType, const scString &path);
  scString genFileName(const scString &nameTpl, uint seqNo);
protected:
  uint m_seqNo;  
  scString m_namePattern;
  scString m_tempNamePfx;
  scString m_workNamePfx;
  scString m_obsolNamePfx;
  scDataNode m_pendingAllocs;
  scString m_currentName;
  scString m_previousName;
  scString m_coreName;
};


#endif // _JOBWORKFILE_H__