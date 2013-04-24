# Lua-to-D-Bus (L2DBUS) Library
---
## Introduction

A gentle introduction to the L2DBUS library can be found <a href="./doc/manual/01-introduction.md">here</a>.

## Dependencies

L2DBUS depends on the following external libraries:

* [D-Bus reference Library](http://dbus.freedesktop.org/releases/dbus/) (version > 1.4.X)
* [libev](http://software.schmorp.de/pkg/libev.html) (version >= 4.00)
* [Lua](http://www.lua.org/download.html) (version 5.1.X or 5.2.X) or [LuaJIT](http://luajit.org/download.html) (version > 2.0.X)
* CDBUS
* [CMake](http://www.cmake.org/) (version >= 2.6.0) Necessary for building.

Linking will also pull in the math library.

## Building

Before building L2DBUS all of it's [dependencies](#Dependencies) must be built and installed in their proper locations.

### Host Build

The L2DBUS build is based on CMake and a convenient script (*build_host.sh*) is provided to simplify the build process. So to build the library (once the dependencies are built and installed) then execute:

	# ./build_host.sh

An "debug" option can be specified as the first argument to make a debug version of the library:

	# ./build_host.sh debug

### Target Build

There is also a script to build a cross-compiled version of the library (*build_target.sh*). This needs to up modified for the particular build environment. In particular, a CMake toolchain file must be specifed (*cmake.toolchain*) which should be located in the top-level path defined by *CMAKE_TOOLCHAIN_PATH_PREFIX*. Furthermore a shell script (*environment-setup*), if present in the same toolchain path, will be sourced prior to building the target library. Additional details on cross-compiling under CMake can be found [here](http://www.vtk.org/Wiki/CMake_Cross_Compiling).

### Specifying the Lua Version

The L2DBUS module can be built against either the default version of Lua (5.1) or a specific version of Lua by setting the CMake variable *L2DBUS_LUA_VERSION*. For instance, to explicitly build against the 5.2 version of Lua the following option can be specified for either the host or target build script.

	# ./build_host.sh -DL2DBUS_LUA_VERSION=5.2

The build will the *require* this version of Lua to be present in the build environment or CMake will fail with an appropriate message.

### Installation

The library installation path prefix (for both host and/or target) can be modified by passing in a CMake variable on the command line:

	# ./build_host.sh -DCMAKE_INSTALL_PREFIX=/usr/local

The default C-module (LUA_CMOD) installation path is set to

*${CMAKE_INSTALL_PREFIX}/lib/lua/${L2DBUS_LUA_VERSION}*

The default Lua module (LUA_LMOD) installation path is set to

*${CMAKE_INSTALL_PREFIX}/share/lua/${L2DBUS_LUA_VERSION}*


To install the library type:

	# make install

To uninstall the library type:

	# make uninstall

To generate public documention type:

	# make pub-docs

To generate *private* internal documentation type:

	# make priv-docs

To install the public documentation type:

	# make install-docs

The public documentation is stored in the *${CMAKE_INSTALL_PREFIX}/share/doc/l2dbus* directory.

The public documents can be uninstalled using either:

	# make uninstall-docs

or

	# make uninstall

Both options will remove the documentation while the last option (uninstall) removing the entire installation as well.

To generate a source distribution type:

	# make dist-l2dbus

And of course to *clean* the build you can simple erase the build directory from the project root:

	# rm -rf x86_64-Release


**TODO:** This needs to be beefed up. Ultimately there needs to be a LuaRocks spec.

## License

The L2DBUS library itself is licensed under the MIT License. See <a href="../../../LICENSE">LICENSE</a> in the source distribution for additional details. Additional terms may apply since L2DBUS does have other dependencies. The intent, however, was to utilize components that only have compatible licenses and support both open source and propriety (closed) source applications.
