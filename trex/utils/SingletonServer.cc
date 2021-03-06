/** @file "SingletonServer.cc"
 * @brief SingletonServer internal class implmentation
 *
 * @author Frederic Py <fpy@mbari.org>
 * @ingroup utils
 */
/*********************************************************************
 * Software License Agreement (BSD License)
 * 
 *  Copyright (c) 2011, MBARI.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TREX Project nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "private/SingletonServer.hh"

#include <boost/thread/once.hpp>

namespace {
  boost::once_flag o_flag = BOOST_ONCE_INIT;
}

using namespace TREX::utils::internal;

// static

SingletonServer *SingletonServer::s_instance = 0x0;

void SingletonServer::make_instance() {
  if( 0x0==s_instance )
    s_instance = new SingletonServer;
}

SingletonServer &SingletonServer::instance() {
  // Use the C++11 like format
  boost::call_once(o_flag, &SingletonServer::make_instance);
  return *s_instance;
}

// *structors

SingletonServer::SingletonServer() {}

SingletonServer::~SingletonServer() {}

// manipulators 

SingletonDummy *SingletonServer::attach(std::string const &id,
                                        sdummy_factory const &factory) {
  // Just to be safe for now
  assert(this==s_instance);
  {
    lock_type lock(m_mtx);
    single_map::iterator i = m_singletons.find(id);
    if( m_singletons.end()==i ) {
      i = m_singletons.insert(single_map::value_type(id, factory.create())).first;
    }
    i->second->incr_ref();
    return i->second;
  }
}

bool SingletonServer::detach(std::string const &id) {
  assert(this==s_instance);
  {
    lock_type lock(m_mtx);
    single_map::iterator i = m_singletons.find(id);
    if( m_singletons.end()!=i ) {
      SingletonDummy *ptr = i->second;
      if( ptr->decr_ref() ) {
        m_singletons.erase(i);
        delete ptr;
        return true;
      }
    }
  }
  return false;
}
