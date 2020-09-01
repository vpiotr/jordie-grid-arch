/////////////////////////////////////////////////////////////////////////////
// Name:        EnvSerializerJsonYajl.h
// Project:     scLib
// Purpose:     Serializer using JSON format & Yajl lib
// Author:      Piotr Likus
// Modified by:
// Created:     19/11/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////


#ifndef _ENVSERIALIZERJSONYAJL_H__
#define _ENVSERIALIZERJSONYAJL_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file EnvSerializerJsonYajl.h
///
/// Probably fastest JSON serializer.

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
//sc
#include "sc/dtypes.h"
//grd
#include "grd/core.h"
#include "grd/Envelope.h"

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
// ----------------------------------------------------------------------------
// scEnvSerializerJsonYajl
// ----------------------------------------------------------------------------
class scEnvSerializerJsonYajl: public scEnvelopeSerializerBase {
public:
  scEnvSerializerJsonYajl();
  virtual ~scEnvSerializerJsonYajl() {};
  virtual int convToString(const scEnvelope& input, scString &output);  
  virtual int convFromString(const scString &input, scEnvelope& output);  
protected:
  int convDataNodeToEnvelope(const scDataNode &input, scEnvelope& output);
};


#endif // _ENVSERIALIZERJSONYAJL_H__