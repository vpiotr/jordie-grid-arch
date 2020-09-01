/////////////////////////////////////////////////////////////////////////////
// Name:        W32NamedPipesQueue.cpp
// Project:     grdLib
// Purpose:     Synchronized message queue for named pipes gates.
// Author:      Piotr Likus
// Modified by:
// Created:     06/10/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd\W32NamedPipesQueue.h"

grdW32NamedPipesQueue::grdW32NamedPipesQueue()
{
}

grdW32NamedPipesQueue::~grdW32NamedPipesQueue()
{
  // wait for unlock before destruction of data
  boost::mutex::scoped_lock l(m_mutex); 
}

size_t grdW32NamedPipesQueue::size() const
{
  boost::mutex::scoped_lock l(m_mutex); 
  return m_messages.size();
}

bool grdW32NamedPipesQueue::empty() const
{
  boost::mutex::scoped_lock l(m_mutex); 
  return m_messages.empty();
}

void grdW32NamedPipesQueue::put(const grdW32NamedPipesMessageIntf &message)
{
  boost::mutex::scoped_lock l(m_mutex); 
  m_messages.push_back(message.clone());
}

bool grdW32NamedPipesQueue::tryGet(grdW32NamedPipesMessageIntf &message)
{
  boost::mutex::scoped_lock l(m_mutex); 
  if (m_messages.empty())
  {
    message.clear();
    return false;
  }

  message.setValue(*m_messages.pop_front());
  return true;
}

bool grdW32NamedPipesQueue::peek(grdW32NamedPipesMessageIntf &message)
{
  boost::mutex::scoped_lock l(m_mutex); 
  if (m_messages.empty())
  {
    message.clear();
    return false;
  }

  message.setValue(m_messages.front());
  return true;
}

void grdW32NamedPipesQueue::eraseTop()
{
  boost::mutex::scoped_lock l(m_mutex); 
  if (!m_messages.empty())
    m_messages.erase(m_messages.begin());
}

