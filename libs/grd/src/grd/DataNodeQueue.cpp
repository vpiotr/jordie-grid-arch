/////////////////////////////////////////////////////////////////////////////
// Name:        grdDataNodeQueue.cpp
// Project:     grdLib
// Purpose:     Synchronized storage queue for messages.
// Author:      Piotr Likus
// Modified by:
// Created:     28/09/2013
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

#include "grd\DataNodeQueue.h"

grdDataNodeQueue::grdDataNodeQueue()
{
  m_data.setAsList();
}

grdDataNodeQueue::~grdDataNodeQueue()
{
  // wait for unlock before destruction of data
  boost::mutex::scoped_lock l(m_mutex); 
}

void grdDataNodeQueue::put(const scDataNode &message)
{
  boost::mutex::scoped_lock l(m_mutex); 
  m_data.addElement(message);
}

bool grdDataNodeQueue::tryGet(scDataNode &output)
{
    boost::mutex::scoped_lock l(m_mutex); 
    bool res = !m_data.empty();
    if (res) {
        std::auto_ptr<scDataNode> outputNode(m_data.extractChild(0));
        output = base::move(*outputNode);
    }
    return res;
}

bool grdDataNodeQueue::peek(scDataNode &output)
{
    boost::mutex::scoped_lock l(m_mutex); 
    bool res = !m_data.empty();
    if (res) {
        m_data.getElement(0, output);
    }
    return res;
}

void grdDataNodeQueue::eraseTop()
{
  boost::mutex::scoped_lock l(m_mutex); 
  if (!m_data.empty())
    m_data.eraseElement(0);
}

bool grdDataNodeQueue::empty()
{
  boost::mutex::scoped_lock l(m_mutex); 
  return m_data.empty();
}

size_t grdDataNodeQueue::size()
{
  boost::mutex::scoped_lock l(m_mutex); 
  return m_data.size();
}
