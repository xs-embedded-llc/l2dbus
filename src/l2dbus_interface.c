/*===========================================================================
 *
 * Project         l2dbus
 *
 * Released under the MIT License (MIT)
 * Copyright (c) 2013 XS-Embedded LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *===========================================================================
 *===========================================================================
 * @file           l2dbus_interface.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of CDBUS interface.
 *===========================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "dbus/dbus.h"
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_interface.h"
#include "l2dbus_serviceobject.h"
#include "l2dbus_core.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_alloc.h"
#include "l2dbus_object.h"
#include "l2dbus_message.h"
#include "l2dbus_dbuscompat.h"
#include "lualib.h"

/**
 L2DBUS Interface

 This section describes a Lua Interface class. This class is used to
 define an interface in D-Bus terms so that services can be offered on
 the D-Bus that support a specific API.

 Once created, interfaces are typically associated with one (or more)
 D-Bus service objects. The interface itself can be configured to
 receive and handle client requests. If **not** handled by the interface
 the associated service object is given the opportunity to handle the
 request.

 @namespace l2dbus.Interface
 */


/**
 * @brief Frees up the elements of an introspection argument.
 */
static void
l2dbus_interfaceDestroyArg
    (
    cdbus_DbusIntrospectArgs* arg
    )
{
    if ( NULL != arg )
    {
        l2dbus_free(arg->name);
        l2dbus_free(arg->signature);
    }
}


/**
 * @brief Frees up the elements of an introspection item.
 *
 * This has a member name and possibly additional arguments.
 */
static void
l2dbus_interfaceDestroyItem
    (
    cdbus_DbusIntrospectItem* item
    )
{
    cdbus_UInt32 idx;

    if ( NULL != item )
    {
        l2dbus_free(item->name);
        for ( idx = 0; idx < item->nArgs; ++idx )
        {
            l2dbus_interfaceDestroyArg(&item->args[idx]);
        }
        l2dbus_free(item->args);
    }
}


/**
 * @brief Frees up the elements of an introspection property.
 *
 * This has a property name and signature.
 */
static void
l2dbus_interfaceDestroyProperty
    (
    cdbus_DbusIntrospectProperty* prop
    )
{
    if ( NULL != prop )
    {
        l2dbus_free(prop->name);
        l2dbus_free(prop->signature);
    }
}


/**
 @brief Handles and processes requests to the interface.

 This function will try to deliver a callback to a Lua handler function
 when invoked. The handler itself will determine whether or not it
 can handle the request.

 @param [in] conn     The CDBUS connection associated with this interface
 request.
 @param [in] obj      The CDBUS service object implementing the interface.
 @param [in] msg      The D-Bus request message (e.g. method call).
 @param [in] userdata Opaque data provided by the client when the handler
 was registered.
 @return A suitable DBusHandlerResult value. Returning
 DBUS_HANDLER_RESULT_HANDLED indicates the interface handler processed
 the request. Otherwise the service object will be given a chance to
 handle the request.
 */
static DBusHandlerResult
l2dbus_interfaceHandler
    (
        struct cdbus_Connection*    conn,
        struct cdbus_Object*        obj,
        DBusMessage*                msg,
        void*                       userdata
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    DBusHandlerResult rc = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    l2dbus_Interface* ud;

    /* Leaves the userdata sitting on the top of the stack */
    ud = l2dbus_objectRegistryGet(L, userdata);

    /* Nil or the Interface userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != ud );
    assert( NULL != L );

    /* If the watch userdata has been GC'ed then ... */
    if ( NULL == ud )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Cannot call interface handler because interface has been GC'ed"));
    }
    else if ( LUA_NOREF != ud->cbCtx.funcRef )
    {
        /* Push function and user value on the stack and execute the callback */
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.funcRef);
        /* Push the interface userdata */
        lua_pushvalue(L, -2 /* Interface ud */);
        /* Push the associated Lua userdata wrapper on the stack */
        l2dbus_objectRegistryGet(L, conn);
        if ( lua_isnil(L, -1) )
        {
            L2DBUS_TRACE((L2DBUS_TRC_WARN, "Cannot call interface handler "
                "because connection has been GC'ed"));
        }
        else
        {
            /* Push a Lua wrapper around the message */
            l2dbus_messageWrap(L, msg, L2DBUS_TRUE);

            lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

            if ( 0 != lua_pcall(L, 4 /* nArgs */, 1 /* nResults */, 0) )
            {
                if ( lua_isstring(L, -1) )
                {
                    errMsg = lua_tostring(L, -1);
                }
                L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Interface callback error: %s",
                                errMsg));
            }
            else
            {
                if ( lua_isnumber(L, -1) )
                {
                    rc = lua_tointeger(L, -1);
                    switch ( rc )
                    {
                        case DBUS_HANDLER_RESULT_HANDLED:
                        case DBUS_HANDLER_RESULT_NOT_YET_HANDLED:
                        case DBUS_HANDLER_RESULT_NEED_MEMORY:
                            /* These are understood */
                            break;

                        default:
                            L2DBUS_TRACE((L2DBUS_TRC_WARN,
                                "Unknown interface callback return code (%d)", rc));
                            rc = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                            break;
                    }
                }
            }
        }
    }

    /* Clean up the thread stack */
    lua_settop(L, 0);

    /* The return value is unused by CDBUS */
    return rc;
}


/**
 @function new

 Creates a new Interface.

 Creates an empty D-Bus Interface used to describe the API of of a service.
 With the interface there can be associated a handler that can process
 client requests. The signature of the handler has the form:

     DBusHandlerResult function onRequest(interface, conn, msg, userToken)

 Where:

 <ul>
 <li>*interface*  - The D-Bus interface associated with the handler</li>
 <li>*conn*       - The D-Bus connection from which the request was received</li>
 <li>*msg*        - The D-Bus request message</li>
 <li>*userToken*  - A value specified by the user when the interface was created.</li>
 </ul>

 The handler function should return one of the following values:

 <ul>
 <li>@{l2dbus.Dbus.HANDLER_RESULT_HANDLED|HANDLER_RESULT_HANDLED} - Request handled</li>
 <li>@{l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED|HANDLER_RESULT_NOT_YET_HANDLED} - Request **not** handled</li>
 <li>@{l2dbus.Dbus.HANDLER_RESULT_NEED_MEMORY|HANDLER_RESULT_NEED_MEMORY} - Request **not** handled due to lack of memory</li>
 </ul>

 For all cases except @{l2dbus.Dbus.HANDLER_RESULT_HANDLED|HANDLER_RESULT_HANDLED} the
 service object (if it has a handler) will be given the opportunity to process the request.
 If there are no handlers to satisfy the request or an error occurs the client will
 receive an appropriate error message.

 @tparam string interface A valid D-Bus interface name to assign to the interface.
 @tparam ?func|nil handler An optional interface handler function or **nil** if none desired.
 @tparam ?any userToken Optional client data associated with the handler. Will be passed
 to the handler when its invoked.
 @treturn userdata The userdata object representing the Interface.
 */
static int
l2dbus_newInterface
    (
    lua_State*  L
    )
{
    l2dbus_Interface* intfUd;
    const char* intfName = NULL;
    int userIdx = L2DBUS_CALLBACK_NOREF_NEEDED;
    int funcIdx = L2DBUS_CALLBACK_NOREF_NEEDED;
    int nArgs = lua_gettop(L);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: interface"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    intfName = luaL_checkstring(L, 1);

    if ( !l2dbus_validateInterface(intfName) )
    {
        luaL_error(L, "invalid D-Bus interface name");
    }

    /* Check for a handler function */
    if ( (nArgs >= 2) &&  (LUA_TFUNCTION == lua_type(L, 2)) )
    {
        funcIdx = 2;
    }

    /* See if an optional user value is provided */
    if ( nArgs >  2 )
    {
        userIdx = 3;
    }

    intfUd = (l2dbus_Interface*)l2dbus_objectNew(L, sizeof(*intfUd),
                                             L2DBUS_INTERFACE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Interface userdata=%p", intfUd));

    if ( NULL == intfUd )
    {
        luaL_error(L, "Failed to create interface userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        l2dbus_callbackInit(&intfUd->cbCtx);

        l2dbus_callbackRef(L, funcIdx, userIdx, &intfUd->cbCtx);
        intfUd->intf = cdbus_interfaceNew(intfName, l2dbus_interfaceHandler, intfUd);

        if ( NULL == intfUd->intf )
        {
            /* Release any references we may still have */
            l2dbus_callbackUnref(L, &intfUd->cbCtx);
            luaL_error(L, "Failed to allocate interface");
        }
        else
        {
            /* Create a (weak) mapping between the interface userdata pointer and
             * itself.
             */
            l2dbus_objectRegistryAdd(L, intfUd, -1);
        }
    }

    return 1;
}


/**
 * @brief Called by Lua VM to GC/reclaim the Interface userdata.
 *
 * This method is called by the Lua VM to reclaim the Interface
 * userdata.
 *
 * @return nil
 *
 */
static int
l2dbus_interfaceDispose
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ud = (l2dbus_Interface*)luaL_checkudata(L, -1,
                                        L2DBUS_INTERFACE_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: interface (userdata=%p)", ud));

    if ( ud->intf != NULL )
    {
        cdbus_interfaceUnref(ud->intf);
    }

    /* Remove the weak association between the interface userdata pointer
     * and itself.
     */
    l2dbus_objectRegistryRemove(L, ud);

    /* Unreference the function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


/**
 * The L2DBUS Interface class.
 * @type Interface
 */

/**
 @function name
 @within Interface

 Tests whether the connection is connected to the bus.

 This method can be called to detect whether or not
 the provided connection is in fact connected to the
 bus.

 @tparam userdata interface The Interface from which to retrieve the name.
 @treturn ?string|nil A string with the interface name or **nil** if it
 is unset.
 */
static int
l2dbus_interfaceGetName
    (
    lua_State*  L
    )
{
    const char* name;
    l2dbus_Interface* ud = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    name = cdbus_interfaceGetName(ud->intf);
    if ( NULL != name )
    {
        lua_pushstring(L, name);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}


/**
 @function setData
 @within Interface

 Sets the user data to be passed to the request handler.

 @tparam userdata interface The Interface to set the user data.
 @tparam any data The user data. Can be any value.
 */
static int
l2dbus_interfaceSetData
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ud = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    luaL_checkany(L, -1);

    /* Unreference the previous value */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

    /* On the top of the stack should be the client's user data value.
     * We'll keep a reference to that for safe-keeping.
     */
    ud->cbCtx.userRef = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}


/**
 @function getData
 @within Interface

 Gets the user data associated with the handler.

 @tparam userdata interface The Interface to get the user data.
 @treturn any The user data associated with the interface handler.
 */
static int
l2dbus_interfaceGetData
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ud = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

    return 1;
}


/**
 This table that defines an individual D-Bus property item in an interface
 description. An individual property is typically one of many in an array
 that is associated with an interface.

Possible values for the *access* field include *r* (read), *w* (write), *rw* or
*wr* (read/write).

 @table IntrospectProp
 @field name (string) [Req] The name of the D-Bus property.
 @field sig (string) [Req] The D-Bus signature of the property.
 @field access (string) [Req] The access permissions for the property (**r**, **w**, or **rw**).
 @see registerProperties
 */


/**
 This table that defines an individual method/signal argument in an interface
 description. An individual argument is typically one of many in an array
 that is associated with a method/signal.

 The field *dir* can be either *in* or *out* but **not** *inout*. For D-Bus
 signal arguments this field isn't required because they are always *out*. For
 method arguments if the direction is omitted then it is assumed to be *in*.

 @table IntrospectArg
 @field name (string) [Opt] The name of the argument.
 @field sig (string) [Req] The D-Bus signature of the argument.
 @field dir (string) [Opt] The *direction* of the argument (**in** or **out**).
 @see registerMethods
 @see registerSignals
 */

/**
 This table that defines a D-Bus method or signal exposed by an interface.
 An **array** of these tables is passed to the @{registerMethods} or
 @{registerSignals} to register the methods/signals an interface exposes.


 @table IntrospectItem
 @field name (string) [Req] The name of the method or signal
 @field args (array) [Opt] An array of arguments of type @{IntrospectArg}
 */


/**
 @brief Parses table of interface items.

 These interface items can either describe methods or signals.

 @param [in]        L           Lua state.
 @param [in]        itemsIdx    Index on Lua stack to the item table.
 @param [in,out]    items       Returns a pointer to an array of
 interface items.
 @param [in,out]    nItems      If non-NULL returns the number of items
 in the 'items' array.
 @param [in]        parseAsMethods  If true parse items as method definitions
 rather than signal definitions.
 @param [in,out]    whyFail     If non-NULL on return it will point to a
 constant string with the reason for the failure. This should *not* be freed.
 @return Returns true if the item was parsed successfully, false otherwise.
 */
static l2dbus_Bool
l2dbus_interfaceParseItems
    (
    lua_State*                  L,
    int                         itemsIdx,
    cdbus_DbusIntrospectItem**  items,
    size_t*                     nItems,
    l2dbus_Bool                 parseAsMethods,
    const char**                whyFail
    )
{
    l2dbus_Bool isValid = L2DBUS_FALSE;
    char* reason = "unknown failure";
    size_t nArgs = 0;
    size_t itemIdx = 0;
    size_t argIdx;
    int argTblRef;
    int stackTop;
    cdbus_DbusIntrospectArgs* args = NULL;
    const char* direction;

    itemsIdx = lua_absindex(L, itemsIdx);
    stackTop = lua_gettop(L);

    if ( NULL == items )
    {
        reason = "bad parameter";
    }
    else if ( NULL == nItems )
    {
        reason = "bad parameter";
    }
    else if ( LUA_TTABLE != lua_type(L, itemsIdx) )
    {
        reason = "unexpected argument (table expected)";
    }
    else
    {
        *nItems = lua_rawlen(L, itemsIdx);
        /* I guess it's valid to register no items */
        if ( 0 == *nItems )
        {
            isValid = L2DBUS_TRUE;
        }
        else
        {
            (*items) = (cdbus_DbusIntrospectItem*)l2dbus_calloc(*nItems,
                                            sizeof(cdbus_DbusIntrospectItem));
            if ( NULL == *items )
            {
                reason = "failed to allocate memory for items";
            }
            else
            {
                for ( itemIdx = 0; itemIdx < *nItems; ++itemIdx )
                {
                    /* Pop any junk on the stack which might've accumulated
                     * since the last iteration.
                     */
                    lua_settop(L, stackTop);

                    lua_rawgeti(L, itemsIdx, itemIdx+1);
                    if ( LUA_TTABLE != lua_type(L, -1) )
                    {
                        /* Unexpected Lua type - bail out */
                        reason = "unexpected (non-table) type found for arg #2";
                        break;
                    }

                    lua_getfield(L, -1, "name");
                    if ( !lua_isstring(L, -1) )
                    {
                        reason = "missing name for method/signal";
                        break;
                    }
                    (*items)[itemIdx].name = l2dbus_strDup(lua_tostring(L, -1));
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "args");
                    if ( LUA_TTABLE == lua_type(L, -1) )
                    {
                        nArgs = lua_rawlen(L, -1);
                    }
                    else
                    {
                        nArgs = 0;
                    }
                    argTblRef = lua_absindex(L, -1);

                    if ( 0 < nArgs )
                    {
                        args = (cdbus_DbusIntrospectArgs*)l2dbus_calloc(nArgs, sizeof(*args));
                        if ( NULL == args )
                        {
                            reason = "memory allocation failure for argument list";
                            break;
                        }
                    }
                    else
                    {
                        args = NULL;
                    }
                    (*items)[itemIdx].args = args;
                    (*items)[itemIdx].nArgs = nArgs;

                    for ( argIdx = 0; argIdx < nArgs; ++argIdx )
                    {
                        lua_rawgeti(L, argTblRef, argIdx+1);
                        if ( LUA_TTABLE != lua_type(L, -1) )
                        {
                            reason = "table expected containing argument description";
                            break;
                        }

                        lua_getfield(L, -1, "name");
                        if ( lua_isstring(L, -1) )
                        {
                            (*items)[itemIdx].args[argIdx].name = l2dbus_strDup(lua_tostring(L, -1));
                        }
                        lua_pop(L, 1);

                        lua_getfield(L, -1, "sig");
                        if ( !lua_isstring(L, -1) )
                        {
                            reason = "argument is missing a signature";
                            break;
                        }
                        if ( !dbus_signature_validate(lua_tostring(L, -1), NULL) )
                        {
                            reason = "invalid signature";
                            break;
                        }

                        (*items)[itemIdx].args[argIdx].signature = l2dbus_strDup(lua_tostring(L, -1));
                        lua_pop(L, 1);

                        /* If we're not parsing methods (e.g. signals instead) */
                        if ( !parseAsMethods )
                        {
                            /* Signals are *always* out */
                            (*items)[itemIdx].args[argIdx].xferDir = CDBUS_XFER_OUT;
                        }
                        else
                        {
                            lua_getfield(L, -1, "dir");
                            if ( !lua_isstring(L, -1) )
                            {
                                /* Default to in */
                                (*items)[itemIdx].args[argIdx].xferDir = CDBUS_XFER_IN;
                            }
                            else
                            {
                                direction = lua_tostring(L, -1);
                                if ( 0 == strcmp(direction, "in") )
                                {
                                    (*items)[itemIdx].args[argIdx].xferDir = CDBUS_XFER_IN;
                                }
                                else if ( 0 == strcmp(direction, "out") )
                                {
                                    (*items)[itemIdx].args[argIdx].xferDir = CDBUS_XFER_OUT;
                                }
                                else
                                {
                                    reason = "unsupported argument direction";
                                    break;
                                }
                            }
                            lua_pop(L, 1);
                        }

                        /* Pop off the argument table */
                        lua_pop(L, 1);
                    }

                    /* If we encountered a problem parsing the arguments then ... */
                    if ( argIdx < nArgs )
                    {
                        break;
                    }
                }
            }

            /* If we parsed the whole thing then ... */
            if ( itemIdx == *nItems )
            {
                isValid = L2DBUS_TRUE;
            }
        }
    }

    if ( !isValid )
    {
        *whyFail = reason;
    }

    /* Reset the top */
    lua_settop(L, stackTop);

    return isValid;
}


/**
 @function registerMethods
 @within Interface

 Registers methods supported by the interface.

 This method registers D-Bus methods exposed by this interface.
 It does **not** support incremental registration of methods one
 at a time. Repeated calls will erase the previous methods before adding
 the new ones. The interface description should be encoded into a
 Lua table with the following structure:
     {
         {                      -- IntrospectItem[1]
             name = "SetPosition",
             args =
                 {
                     {          -- IntrospectArg[1]
                     name = "x",
                     sig = "i",
                     dir = "in"
                     },
                     {          -- IntrospectArg[2]
                     name = "y"
                     sig = "i",
                     dir = "in"
                     },
                     ...        -- IntrospectArg[N]
                  }
          },
          ...                   -- IntrospectItem[N]
      }


 See @{IntrospectItem} and @{IntrospectArg} for the definition of the
 individual elements of the introspection table. Any error parsing or
 registering the methods will result in a Lua error being thrown.

 @tparam userdata interface The Interface on which to register methods.
 @tparam table methods The introspection data for the methods being
 registered with this interface.
 */
static int
l2dbus_interfaceRegisterMethods
    (
    lua_State*  L
    )
{
    size_t nMethods = 0;
    size_t methIdx;
    cdbus_DbusIntrospectItem* methods = NULL;
    const char* reason = "";

    l2dbus_Bool isRegistered = L2DBUS_FALSE;
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TTABLE);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    if ( l2dbus_interfaceParseItems(L, 2, &methods, &nMethods, L2DBUS_TRUE, &reason) )
    {
        isRegistered = cdbus_interfaceRegisterMethods(ifUd->intf, methods, nMethods);
        if ( !isRegistered )
        {
            reason = "failed to register methods in CDBUS";
        }
    }

    /* Always free up the methods */
    if ( NULL != methods )
    {
        for ( methIdx = 0; methIdx < nMethods; ++methIdx )
        {
            l2dbus_interfaceDestroyItem(&methods[methIdx]);
        }
        l2dbus_free(methods);
    }

    if ( !isRegistered )
    {
        luaL_error(L, reason);
    }

    return 0;
}


/**
 @function clearMethods
 @within Interface

 Clears or erases the methods supported by the interface.

 @tparam userdata interface The Interface on which to clear the methods.
 @treturn bool Returns **true** if the methods are cleared successfully,
 **false** otherwise.
 */
static int
l2dbus_interfaceClearMethods
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                            L2DBUS_INTERFACE_MTBL_NAME);
    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushboolean(L, cdbus_interfaceClearMethods(ifUd->intf));

    return 1;
}


/**
 @function registerSignals
 @within Interface

 Registers signal emitted by the interface.

 This method registers D-Bus signals emitted by this interface.
 It does **not** support incremental registration of signals one
 at a time. Repeated calls will erase the previous signals before adding
 the new ones. The description of the signals should be encoded into a
 Lua table with the following structure:
     {
         {                      -- IntrospectItem[1]
             name = "PositionUpdate",
             args =
                 {
                     {          -- IntrospectArg[1]
                     name = "x",
                     sig = "i",
                     },
                     {          -- IntrospectArg[2]
                     name = "y"
                     sig = "i",
                     },
                     ...        -- IntrospectArg[N]
                  }
          },
          ...                   -- IntrospectItem[N]
      }


 See @{IntrospectItem} and @{IntrospectArg} for the definition of the
 individual elements of the introspection table. Any error parsing or
 registering the signals will result in a Lua error being thrown.

 @tparam userdata interface The Interface on which to register signals.
 @tparam table signals The introspection data for the signals being
 registered with this interface.
 */
static int
l2dbus_interfaceRegisterSignals
    (
    lua_State*  L
    )
{
    size_t nSignals = 0;
    size_t sigIdx;
    cdbus_DbusIntrospectItem* signals = NULL;
    const char* reason = "";

    l2dbus_Bool isRegistered = L2DBUS_FALSE;
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TTABLE);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    if ( l2dbus_interfaceParseItems(L, 2, &signals, &nSignals, L2DBUS_FALSE, &reason) )
    {
        isRegistered = cdbus_interfaceRegisterSignals(ifUd->intf, signals, nSignals);
        if ( !isRegistered )
        {
            reason = "failed to register signals in CDBUS";
        }
    }

    /* Always free up the signals */
    if ( NULL != signals )
    {
        for ( sigIdx = 0; sigIdx < nSignals; ++sigIdx )
        {
            l2dbus_interfaceDestroyItem(&signals[sigIdx]);
        }
        l2dbus_free(signals);
    }

    if ( !isRegistered )
    {
        luaL_error(L, reason);
    }

    return 0;
}


/**
 @function clearSignals
 @within Interface

 Clears or erases the signals supported by the interface.

 @tparam userdata interface The Interface on which to clear the signals.
 @treturn bool Returns **true** if the signals are cleared successfully,
 **false** otherwise.
 */
static int
l2dbus_interfaceClearSignals
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                            L2DBUS_INTERFACE_MTBL_NAME);
    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushboolean(L, cdbus_interfaceClearSignals(ifUd->intf));

    return 1;
}


/**
 @function registerProperties
 @within Interface

 Registers properties supported by the interface.

 This method registers D-Bus properties supported by this interface.
 It does **not** support incremental registration of properties one
 at a time. Repeated calls will erase the previous properties before adding
 the new ones. The properties should be encoded into a Lua table with the
 following structure:
     {
         {                      --  IntrospectProp[1]
             name = "Time",
             sig = "i",
             access = "r"
         },
         {                      --  IntrospectProp[2]
             name = "Velocity",
             sig = "d",
             access = "rw"
         },
         ...                    --  IntrospectProp[N]
      }


 See @{IntrospectProp} for the definition of the individual elements of the
 introspective properties table. Any error parsing or
 registering the properties will result in a Lua error being thrown. Although
 these properties will appear as part of the D-Bus Introspection data if a
 client should introspect a service implementing this interface, it is implied
 that the service must also implement the
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties">Properties</a>
 interface in order for the client to directly access them.

 @tparam userdata interface The Interface on which to register properties.
 @tparam table properties The introspection data for the properties being
 registered with this interface.
 */
static int
l2dbus_interfaceRegisterProperties
    (
    lua_State*  L
    )
{
    size_t  nProps;
    size_t  propIdx;
    const char* access;
    l2dbus_Bool isRegistered = L2DBUS_FALSE;
    const char* reason = "unknown failure";
    cdbus_DbusIntrospectProperty* props = NULL;
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);

    luaL_checktype(L, 2, LUA_TTABLE);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    nProps = lua_rawlen(L, 2);
    /* I guess it's valid to register no items */
    if ( 0 == nProps )
    {
        isRegistered = L2DBUS_TRUE;
    }
    else
    {
        props = (cdbus_DbusIntrospectProperty*)l2dbus_calloc(nProps, sizeof(*props));
        if ( NULL == props )
        {
            reason = "failed to allocate memory for properties";
        }
        else
        {
            for ( propIdx = 0; propIdx < nProps; ++propIdx )
            {
                lua_rawgeti(L, 2, propIdx+1);
                if ( LUA_TTABLE != lua_type(L, -1) )
                {
                    /* Unexpected Lua type - bail out */
                    reason = "unexpected (non-table) type found for arg #2";
                    break;
                }

                lua_getfield(L, -1, "name");
                if ( !lua_isstring(L, -1) )
                {
                    reason = "missing property name";
                    break;
                }
                props[propIdx].name = l2dbus_strDup(lua_tostring(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "sig");
                if ( !lua_isstring(L, -1) )
                {
                    reason = "missing signature";
                    break;
                }
                props[propIdx].signature = l2dbus_strDup(lua_tostring(L, -1));
                if ( !dbus_signature_validate(props[propIdx].signature, NULL) )
                {
                    reason = "invalid signature";
                    break;
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "access");
                if ( !lua_isstring(L, -1) )
                {
                    reason = "missing access rights";
                    break;
                }
                access = lua_tostring(L, -1);
                if ( (0 == strncmp(access, "r", 1)) || (0 == strncmp(access, "rw", 2)) ||
                    (0 == strncmp(access, "wr", 2)) )
                {
                    props[propIdx].read = CDBUS_TRUE;
                }
                if ( (0 == strncmp(access, "w", 1)) || (0 == strncmp(access, "rw", 2)) ||
                    (0 == strncmp(access, "wr", 2)) )
                {
                    props[propIdx].write = CDBUS_TRUE;
                }

                if ( !props[propIdx].write && !props[propIdx].read )
                {
                    reason = "property must have read/write or both access";
                    break;
                }

                /* Pop the last field and the property table */
                lua_pop(L, 2);
            }

            /* If all the properties were assigned then */
            if ( propIdx == nProps )
            {
                isRegistered = cdbus_interfaceRegisterProperties(ifUd->intf, props, nProps);
            }
        }
    }

    /* Always free up the signals */
    if ( NULL != props )
    {
        for ( propIdx = 0; propIdx < nProps; ++propIdx )
        {
            l2dbus_interfaceDestroyProperty(&props[propIdx]);
        }
        l2dbus_free(props);
    }

    if ( !isRegistered )
    {
        luaL_error(L, reason);
    }

    return 0;
}


/**
 @function clearProperties
 @within Interface

 Clears or erases the properties supported by the interface.

 @tparam userdata interface The Interface on which to clear the properties.
 @treturn bool Returns **true** if the properties are cleared successfully,
 **false** otherwise.
 */
static int
l2dbus_interfaceClearProperties
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                            L2DBUS_INTERFACE_MTBL_NAME);
    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushboolean(L, cdbus_interfaceClearProperties(ifUd->intf));

    return 1;
}


/**
 @function introspect
 @within Interface

 Returns the D-Bus XML introspection data for this interface only.

 This isn't usually called directly by client code but is is collected
 by a service implementing the
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-introspectable">Introspectable</a> interface.
 See @{l2dbus.Introspection} for more details.

 @tparam userdata interface The Interface to be introspected.
 @treturn string Returns D-Bus XML introspection data if the interface has
 registered methods, signals, or properties, otherwise **nil**.
 */
static int
l2dbus_interfaceIntrospect
    (
    lua_State*  L
    )
{
    l2dbus_Interface* ud = (l2dbus_Interface*)luaL_checkudata(L, 1,
                                        L2DBUS_INTERFACE_MTBL_NAME);
    cdbus_StringBuffer* buf;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    buf = cdbus_interfaceIntrospect(ud->intf);
    if ( cdbus_stringBufferIsEmpty(buf) )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, cdbus_stringBufferRaw(buf));
        cdbus_stringBufferUnref(buf);
    }

    return 1;
}


/*
 * Define the methods of the Interface class
 */
static const luaL_Reg l2dbus_interfaceMetaTable[] = {
    {"name", l2dbus_interfaceGetName},
    {"setData", l2dbus_interfaceSetData},
    {"data", l2dbus_interfaceGetData},
    {"registerMethods", l2dbus_interfaceRegisterMethods},
    {"clearMethods", l2dbus_interfaceClearMethods},
    {"registerSignals", l2dbus_interfaceRegisterSignals},
    {"clearSignals", l2dbus_interfaceClearSignals},
    {"registerProperties", l2dbus_interfaceRegisterProperties},
    {"clearProperties", l2dbus_interfaceClearProperties},
    {"introspect", l2dbus_interfaceIntrospect},
    {"__gc", l2dbus_interfaceDispose},
    {NULL, NULL},
};


/**
 * @brief Creates the Interface sub-module.
 *
 * This function creates a metatable entry for the Interface userdata
 * and simulates opening the Interface sub-module.
 *
 * @return A table defining the Interface sub-module.
 *
 */
void
l2dbus_openInterface
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_INTERFACE_TYPE_ID,
        l2dbus_interfaceMetaTable));
    lua_newtable(L);
    lua_pushcfunction(L, l2dbus_newInterface);
    lua_setfield(L, -2, "new");
}



