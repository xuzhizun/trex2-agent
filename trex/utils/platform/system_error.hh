/*********************************************************************
 * Software License Agreement (BSD License)
 * 
 *  Copyright (c) 2013, Frederic Py.
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
#ifndef H_trex_config_system_error
# define H_trex_config_system_error

# include "bits/cpp11.hh"

# ifdef DOXYGEN

#  define ERROR_CODE     computed_type
#  define ERROR_CATEGORY computed_type
#  define ERRC           computed_type
#  define SYSTEM_ERROR   computed_type
#  define ERROR_NS       computed_ns

# else // !DOXYGEN

#  ifdef CPP11_HAS_SYSTEM_ERROR

#   include <system_error>
#   define ERROR_NS ::std

#  else // !CPP11_HAS_SYSTEM_ERROR

#   include <boost/system/system_error.hpp>
#   define ERROR_NS ::boost::system

#  endif // CPP11_HAS_SYSTEM_ERROR

#  define ERROR_CODE     ERROR_NS::error_code
#  define SYSTEM_ERROR   ERROR_NS::system_error
#  define ERROR_CATEGORY ERROR_NS::error_category
#  define ERRC           ERROR_NS::errc

# endif // DOXYGEN

#endif // H_trex_config_system_error
