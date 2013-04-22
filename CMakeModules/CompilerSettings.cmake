# Borrowed from: https://github.com/ahornung/octomap-release

# Copyright (c) 2009-2012, K. M. Wurm, A. Hornung, University of Freiburg
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the University of Freiburg nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# COMPILER SETTINGS (default: Release)
# Use "-DCMAKE_BUILD_TYPE=Debug" in cmake for a Debug-build
IF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE Release)
ENDIF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

MESSAGE ("\n")
MESSAGE (STATUS "${PROJECT_NAME} building as ${CMAKE_BUILD_TYPE}")

# COMPILER FLAGS
IF (CMAKE_COMPILER_IS_GNUCC)
  SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-error")
  SET (CMAKE_C_FLAGS_RELEASE "-O3 -fmessage-length=0 -fno-strict-aliasing -DNDEBUG")
  SET (CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DTRACE=1")
  SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-error")
  SET (CMAKE_CXX_FLAGS_RELEASE "-O3 -fmessage-length=0 -fno-strict-aliasing -DNDEBUG")
  SET (CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DTRACE=1")
  # Shared object compilation under 64bit (vtable)
  ADD_DEFINITIONS(-fPIC)
  ADD_DEFINITIONS(-DL2DBUS_MAJOR_VERSION=${L2DBUS_MAJOR_VERSION})
  ADD_DEFINITIONS(-DL2DBUS_MINOR_VERSION=${L2DBUS_MINOR_VERSION})
  ADD_DEFINITIONS(-DL2DBUS_RELEASE_VERSION=${L2DBUS_RELEASE_VERSION})
ENDIF()


#
# See http://www.paraview.org/Wiki/CMake_RPATH_handling for additional details
#

# Use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# When building, don't use the install RPATH already (but later on
# when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# The RPATH to use when installing. Since it's empty the library must
# be found in a path search by other means (e.g. LD_LIBRARY_PATH, default
# library location, etc...)
set(CMAKE_INSTALL_RPATH "")

# Don't add teh automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# no prefix needed for Lua modules
# set(CMAKE_SHARED_MODULE_PREFIX "")