/////////////////////////////////////////////////////////////////////////////
// Name:        ZeroMQGates.h
// Project:     grdLib
// Purpose:     ZeroMQ message gates
// Author:      Piotr Likus
// Modified by:
// Created:     10/04/2010
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#ifndef _ZEROMQGATES_H__
#define _ZEROMQGATES_H__

// ----------------------------------------------------------------------------
// Description
// ----------------------------------------------------------------------------
/// \file core.h
///
/// File description

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------
#include "sc/dtypes.h"
#include "grd/core.h"
#include "grd/GateFactory.h"

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

// abstract
class zmContextBase {
public:
  zmContextBase() {}
  virtual ~zmContextBase() {}
  static zmContextBase *newContext();
  virtual void clear() {}
};

class zmGateFactory: public scGateFactory {
public:
  zmGateFactory(zmContextBase *ctx);
  virtual ~zmGateFactory();
  virtual scMessageGate *createGate(bool input, const scDataNode &params, const scString &protocol) const;
protected:
  zmContextBase *m_context;  
};

class zmGateFactoryForTcp: public zmGateFactory {
public:
  zmGateFactoryForTcp(zmContextBase *ctx):zmGateFactory(ctx) {};
  virtual scMessageGate *createGate(bool input, const scDataNode &params, const scString &protocol) const;
};

class zmGateFactoryForPgm: public zmGateFactory {
public:
  zmGateFactoryForPgm(zmContextBase *ctx):zmGateFactory(ctx) {};
  virtual scMessageGate *createGate(bool input, const scDataNode &params, const scString &protocol) const;
};

class zmGateFactoryForIpc: public zmGateFactory {
public:
  zmGateFactoryForIpc(zmContextBase *ctx):zmGateFactory(ctx) {};
  virtual scMessageGate *createGate(bool input, const scDataNode &params, const scString &protocol) const;
};

#endif // _ZEROMQGATES_H__