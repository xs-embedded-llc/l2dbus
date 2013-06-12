## Introduction
---
### Purpose

Lua-to-D-Bus (L2DBUS) is a Lua module that provides a *nearly* complete binding to the D-Bus IPC service. It is not, however, a complete re-implementation of the D-Bus protocol in Lua but instead leverages the tested D-Bus reference library which implements the lower-level protocol stack. What L2DBUS does provide, however, is access to lower-level features of D-Bus along with a higher level abstraction to make developing Lua D-Bus clients and services easier. These capabilities make it entirely feasible to re-implement a low level program like *dbus-monitor* in Lua as well more complex client/server applications. In this respect the Lua binding is similar to the popular D-Bus Python binding without the implicit GLib/GObject dependency. Hopefully L2DBUS strikes a balance between both low and high level APIs so that there really isn't any D-Bus related feature that is not available to the developer.

The library was originally developed under Lua 5.1 but was designed so that it builds under Lua 5.2 or LuaJIT. Testing under Lua 5.2 and LuaJIT, however, has not been done (*yet*). This library was designed for embedded system environments (Linux) but that should not prevent it from being used in the desktop environments either. Unfortunately, at this time Windows is not supported since it is a rare combination to be running D-Bus on a Microsoft operating system.

### High Level Architecture

<center>![High Level L2DBUS Architecture](./l2dbus_architecture.png?raw=true)</center>

The L2DBUS Lua module is actually composed of several components. These components, from the bottom up, are described as follows:

   * **D-Bus Reference Library**: Low-level "C" reference library designed for language binding courtesy of Freedesktop.org.
   
   * **CDBUS**: A thin "veneer" over the D-Bus reference library the provides useful abstractions. This library can be used directly for "C" D-Bus application development. Main-loop integration is provided by two different (optional) back-ends: libev and Glib. A framework exists to introduce other back-ends as needed (e.g. ecore, Qt, etc...).
   
   * **L2DBUS Core**: Core Lua "C" module that binds with the D-Bus reference library and CDBUS. Written in pure "C". Main loop intregration is carried forward from CDBUS with two back-ends currently supported: libev and Glib. Both main loops can optionally be compiled. L2DBUS "core" isn't directly dependent on either main loop implementation.
   
   * **Lua L2DBUS**: A Lua layer providing higher level D-Bus service/proxy abstractions.
   
   * **Lua Applications**: Lua applications can implement both D-Bus services and clients using synchronous and asynchronous D-Bus programming models.

### Threading

The library itself is single-threaded and includes support for main-loop integration using different back-ends. Initially two main loop back-ends are supported: [libev](http://software.schmorp.de/pkg/libev.html) and [Glib](https://developer.gnome.org/glib/). Both synchronous and asynchronous D-Bus features are available where the asynchronous features work nicely with Lua's native cooperative, coroutine based threading. It is **not** necessary to introduce operating system threads (e.g. pthreads) into an application in order to implement D-Bus clients or services (or both). Fundamentally, the underlying D-Bus reference library only partially supports a threaded OS environment and Lua out-of-the-box doesn't explicitly address this use case. The recommendation, therefore, is **not** to pursue such an approach without great care and full understanding of the code (*there be dragons here*).

### Modules

The L2DBUS package actually consists of several separate modules. Only one (*l2dbus*) represents the core module and contains both the lower level D-Bus functions along with several useful abstractions (e.g. Timer, Watch, Interface, etc...) and a D-Bus *type* system for accurately marshalling/unmarshalling code to and from D-Bus messages. Within the L2DBUS *core* module these functions and classes are placed into separate logical namespaces beneath the module name. To access these namespaces from code you can do something like:

	local l2dbus = require("l2dbus")
	local timer = l2dbus.Timer.new(...)

There are two separate (and optional) Lua modules that implement the main loop back-end. Currently, two different back-ends are supported: [libev](http://software.schmorp.de/pkg/libev.html) and [Glib](https://developer.gnome.org/glib/). When the L2DBUS package is built these two main loop back-ends can optionally be built if the necessary dependencies are satisfied. Regardless, you need at least one main-loop back-end in order to instantiate a Dispatcher instance that runs the main loop. If built, a main loop can be instantiated as shown:

    local mainLoop = require("l2dbus_ev").MainLoop.new()

or

    local mainLoop = require("l2dbus_glib").MainLoop.new()
 
The remaining modules are written in pure Lua and generally provide convenience functions or abstractions over D-Bus client proxies and services. Since these are modules they need to be *required* separately:

	local xml = require("l2dbus.xml")

The L2DBUS modules do **not** pollute the global namespace when required. Instead the result of the Lua *require* should be associated with a Lua variable as shown in the previous examples.

### Examples

The **test** directory of the project contains several example programs that illustrate how to use the binding to write real programs. Most of these were developed to test the functionality of the binding and are not *production* quality implementations. Nonetheless, caveats aside, they do provide helpful examples. 



