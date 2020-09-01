/////////////////////////////////////////////////////////////////////////////
// Name:        NodeFactory.cpp
// Project:     grdLib
// Purpose:     Public interface to global processing node factory.
// Author:      Piotr Likus
// Modified by:
// Created:     02/06/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd/NodeFactory.h"

// ----------------------------------------------------------------------------
// scNodeFactory
// ----------------------------------------------------------------------------
scNodeFactory* scNodeFactory::m_activeFactory = SC_NULL;

scNodeFactory::scNodeFactory()
{
  if (m_activeFactory != SC_NULL)
    throw scError("Node factory already created!");
  m_activeFactory = this;  
}

scNodeFactory::~scNodeFactory()
{
  if (m_activeFactory == this)
    m_activeFactory = SC_NULL;
}

scSchedulerIntf *scNodeFactory::createNode(const scString &a_className, const scString &nodeName)
{
  return (checkFactory()->intCreateNode(a_className, nodeName));
}

scNodeFactory *scNodeFactory::checkFactory()
{
  if (m_activeFactory == SC_NULL)
    throw scError("Node factory not ready!");
  return m_activeFactory;  
}
  
bool scNodeFactory::factoryExists()
{
  return (m_activeFactory != SC_NULL);
}
