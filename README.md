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

Before building L2DBUS all of it's [dependencies](#Dependencies) must be built and installed in their proper locations. The L2DBUS build is based on CMake and a convenient script (*build_host.sh*) is provided to simplify the build process. So to build the library (once the dependencies are built and installed) then execute:

	# ./build_host.sh

An "debug" option can be specified as the first argument to make a debug version of the library:

	# ./build_host.sh debug

There is also a script to build a cross-compiled version of the Lua module. This needs to up modified for a particular build environment.

The library installation path can be modified by specifying passing in a CMake variable on the command line:

	# ./build_host.sh -DINSTALL_LIBS="/path/to/lua/libs"

The default library installation path is set to */usr/local/lib/lua/5.1* 

**TODO:** This path configuration is probably incorrect. There must be a better way to do this under CMake. Plus we need to be able to install the Lua files as well associated with this library.

## Installing

Once the L2DBUS module is built it can be installed using:

	# make install

**TODO:** This needs to be beefed up. Ultimately there needs to be a LuaRocks spec.

## License

The L2DBUS library itself is licensed under the Apache License Version 2.0. See <a href="../../../LICENSE">LICENSE</a> in the source distribution for additional details. Additional terms may apply since L2DBUS does have other dependencies. The intent, however, was to utilize components that only have compatible licenses and support both open source and propriety (closed) source applications.
