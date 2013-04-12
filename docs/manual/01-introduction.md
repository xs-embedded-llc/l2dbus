## Introduction
---
### Purpose

Lua-to-D-Bus (L2DBUS) is a Lua module that provides a *nearly* complete binding to the D-Bus IPC service. It is not, however, a complete re-implementation of the D-Bus protocol in Lua but rather leverages the hardened D-Bus reference library which implements the lower-level protocol stack. What L2DBUS does provide, however, is both access to some of these lower-level features of D-Bus along with a higher level abstraction to make developing Lua D-Bus clients and services easier. In that sense it's entirely feasible to re-implement a low level program like *dbus-monitor* in Lua as well more complex applications. In this respect the Lua binding is akin to the popular D-Bus Python binding without the GLib/GObject dependency. Hopefully L2DBUS strikes a balance between both low and high level APIs so that there really isn't any D-Bus related feature that is not available to the developer.

The library was originally developed under Lua 5.1 but was designed so that it *should* build under Lua 5.2 or LuaJIT. This library was designed for embedded system environments (Linux) but that should not prevent it from being used in customary desktop environments either. Unfortunately, at this time Windows is not supported since it is a rare combination to be running D-Bus on a Microsoft operating system.

### Threading

The library itself is single-threaded and includes support for main-loop integration. The main loop itself it implemented using the [libev](http://software.schmorp.de/pkg/libev.html) library. Both synchronous and asynchronous D-Bus features are available where the asynchronous features work nicely with Lua's native cooperative coroutine based threading. It should **not** be necessary to introduce operating system threads (e.g. pthreads) into an application in order to implement D-Bus clients or services (or both). The underlying D-Bus reference library is not (*really*) written to support a fully OS threaded environment and Lua out-of-the-box doesn't explicitly address this use case. The recommendation, however, is *not* to pursue such an approach without great care and full understanding of the code (*there be dragons here*).

### Modules

The L2DBUS package actually consists of several separate modules. Only one (*l2dbus*) represents the core module and contains both the lower level D-Bus functions along with several useful abstractions (e.g. Timer, Watch, Interface, etc...) and a D-Bus *type* system for accurately marshalling/unmarshalling code to and from D-Bus messages. These functions and classes are placed into separate logical namespaces beneath the module name. To access these namespaces from code you can do something like:

	> local l2dbus = require("l2dbus")
	> local timer = l2dbus.Timer.new(...)

The remaining modules are written in pure Lua and generally provide convenience functions or abstractions over D-Bus client proxies and services. Since these are modules they need to be *required* separately:

	> local xml = require("l2dbus.xml")

The L2DBUS module does **not** pollute the global namespace when required. Instead of associated the core L2DBUS module to the local variable *l2dbus* it could have just as easily been associated with the variable *core* instead.



