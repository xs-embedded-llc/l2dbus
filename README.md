# Lua-to-D-Bus (L2DBUS) Library
---
## Introduction

A gentle introduction to the L2DBUS library can be found <a href="./doc/manual/01-introduction.md">here</a>.

## Dependencies

L2DBUS uses the following external libraries and programs:

   * [D-Bus reference Library](http://dbus.freedesktop.org/releases/dbus/) (version > 1.4.X)
   * [libev](http://software.schmorp.de/pkg/libev.html) (version >= 4.00) **Optional**
   * [Glib](https://developer.gnome.org/glib/) (version >= 2.0.0) **Optional**
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

## Known Issues / Caveats

There are several known issues or caveats to be aware of for this package:

   * Although this package compiles under Lua 5.2, it has *not* been tested or run under Lua 5.2. To date the primary development platform and system is Lua 5.1 based. It *should* work under Lua 5.2 but your mileage may vary. Please help us make the package better by providing feedback on any issues that are encountered when building/running L2DBUS under Lua 5.2 and/or LuaJIT.
   
   * This package contains a hack/kludge to work around a known Lua bug impacting load-able libraries. Specifically, the bug impacts dynamically loaded C-modules with finalizers that may run at program termination. Prior to Lua 5.2.1 it is possible that the Lua GC may *unload* modules before all finalizers have had an opportunity run (and potentially call code that resides in those modules). This typically results in a segmentation fault when a program is terminating. For more information see the full bug description and Lua VM patch [here](http://www.lua.org/bugs.html#5.2.2-1). Since most people won't patch their Lua VM for one reason or another, a work-around was devised. This work-around has been tested under Linux but it should be applicable to MacOSX and Windows. Shared libraries under all these major platforms are reference counted when loaded. This means, for instance, under Linux dlopen() increments a reference and dlclose() decrements it. When Lua loads a C-module the reference is incremented by one. In order to prevent certain modules from being unloaded prematurely (e.g. the *main loop* modules for libev and glib) each module internally loads themselves thus incrementing the reference count. This means even though the Lua GC unloads these C-modules at program termination they won't actually be unmapped from memory until the entire program exits.

   * The *order* in which Lua C modules are garbage collected and unloaded impacts how you can utilize a Lua libev main loop. The Lua binding to [libev](https://github.com/brimworks/lua-ev) allows you to either use the *default* main loop or instantiate a new one. These loops can be used to instantiate an L2DBUS libev-flavored Main Loop which is ultimately used by a Dispatcher object. Unfortunately, if an "instantiated" libev main loop is used (one created by a *ev.Loop.new()* call) during shutdown it's likely the Lua libev module has aleady been unloaded and the main loop destroyed (even if L2DBUS maintains a reference to it). This results in a libev assertion being fired when the application termiantes. To avoid this only the **default** Lua libev main loop should be passed to a L2DBUS libev-flavored Main Loop. This reason this works is because the default libev loop is static and is never destroyed prior to L2DBUS being unloaded by Lua. So, if you chose to borrow a main loop from Lua libev to be used by the L2DBUS Main Loop, choose the default one (*ev.Loop.default*) over one instantiated via *ev.Loop.new()*.
      
   * It is possible when to exceeded a D-Bus connections maximum received size limit (default = 63 MByte) if a client/server pair are exchanging **very large** messages (e.g. payloads > 256KByte) very frequently. Realize, however, this is abusing the intent/purpose of D-Bus which is predominantly intended for use in command/control type applications and services with small messages. What happens is the Lua userdata objects that wraps D-Bus method call/return messages will only unreference a D-Bus message when the Lua userdata wrapper is collected by the Lua garbage collector (GC). If the Lua GC is not running frequently enough (compared to the incoming/outgoing rate of messages) then the number of "active" messages (from the D-Bus library's perspective) can pile up and exceed the connection maximum. At that point all communication over that connection stops. There are a couple potential work-arounds. One option is to manually invoke Lua's garbage collector periodically. Likewise it's possible to configure the garbage collector to run more frequently (see the Lua documentation) so that it runs constantly back-to-back after completing a mark-and-sweep. Another option is to increase the connection maximum size limit (this is configurable from Lua). It's also possible to ask the connection to *estimate* the number of bytes consumed by "active" messages and force a GC when it approaches an application specific threshold. The final option, of course, is to not blast the D-Bus with huge messages.
   
