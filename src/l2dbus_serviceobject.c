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
 * @file           l2dbus_serviceobject.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus service object
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
#include "l2dbus_serviceobject.h"
#include "l2dbus_interface.h"
#include "l2dbus_connection.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_alloc.h"
#include "l2dbus_message.h"
#include "l2dbus_dbuscompat.h"
#include "lualib.h"

/**
 L2DBUS ServiceObject

 This section describes a Lua ServiceObject class.

 A D-Bus service object is an instance of a D-Bus object that appears
 on the bus with a specific object path. Typically a service object will
 implement one or more @{l2dbus.Interface|interfaces} which define the
 methods, properties, and signals exposed by the service (e.g. its
 application programming interface, or API). The handling of requests for a
 service object from client applications can be delegated to the interfaces
 themselves since they can handle request from more than one service object.
 Interfaces can be *shared* by multiple service objects. Likewise service
 object themselves can be *shared* across more than one
 @{l2dbus.Connection|connection}. In this way it's possible for a service
 object to span multiple buses (e.g. have the same object appear on both a
 @{l2dbus.Dbus.BUS_SYSTEM|system} and @{l2dbus.Dbus.BUS_SESSION|session} bus).

 If the interfaces themselves don't have handlers to process requests (or
 they chose not to handle a specific request) then any registered *object*
 handler gets the opportunity to process the request. If no suitable handler
 is found then an appropriate error message is returned to the client
 application.

 **Note:** Although possible, a more convenient method is available to
 implement custom D-Bus services. The @{l2dbus.service} module provides a
 friendlier interface for instantiating D-Bus services and internally leverages
 the ServiceObject. It is recommended that new services are implemented
 using @{l2dbus.service} methods instead.

 @namespace l2dbus.ServiceObject
 */


/**
 @brief Handles and processes requests to the service object.

 This function will try to deliver a callback to a Lua handler function
 when invoked. The handler itself will determine whether or not it
 can handle the request.

 @param [in] obj      The CDBUS service object.
 @param [in] conn     The CDBUS connection associated with this object.
 @param [in] msg      The D-Bus request message (e.g. method call).

 @return A suitable DBusHandlerResult value. Returning
 DBUS_HANDLER_RESULT_HANDLED indicates the object handler processed
 the request. Otherwise a suitable D-Bus error message is ultimately
 returned to the client application.
 */
static DBusHandlerResult
l2dbus_serviceObjectHandler
    (
        struct cdbus_Object*        obj,
        struct cdbus_Connection*    conn,
        DBusMessage*                msg
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    DBusHandlerResult rc = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    l2dbus_ServiceObject* ud;

    /* Leaves the userdata sitting on the top of the stack */
    ud = l2dbus_objectRegistryGet(L, obj);

    /* Nil or the ServiceObject userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != ud );
    assert( NULL != L );

    /* If the watch userdata has been GC'ed then ... */
    if ( NULL == ud )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Cannot call handler because service object has been GC'ed"));
    }
    /* Else if a default callback function was provided then ... */
    else if ( LUA_NOREF != ud->cbCtx.funcRef )
    {
        /* Push function and user value on the stack and execute the callback */
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.funcRef);
        /* Push the service object userdata */
        lua_pushvalue(L, -2 /* Service object ud */);
        /* Push the associated Lua connection userdata wrapper on the stack */
        l2dbus_objectRegistryGet(L, conn);
        if ( lua_isnil(L, -1) )
        {
            L2DBUS_TRACE((L2DBUS_TRC_WARN,
                        "Cannot call object handler because connection has been GC'ed"));
        }
        else
        {
            /* Push a Lua wrapper around the message */
            l2dbus_messageWrap(L, msg, L2DBUS_TRUE);

            /* Push the user provided value on the stack */
            lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

            if ( 0 != lua_pcall(L, 4 /* nArgs */, 1 /* nResults */, 0) )
            {
                if ( lua_isstring(L, -1) )
                {
                    errMsg = lua_tostring(L, -1);
                }
                L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Service object callback error: %s", errMsg));
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
                            L2DBUS_TRACE((L2DBUS_TRC_ERROR,
                                "Unknown service object callback return code (%d)", rc));
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
 @brief Removes an interface from the underlying CDBUS object.

 @param [in] item       The Lua Interface userdata.
 @param [in] userdata   The Lua ServiceObject userdata.
 */
static void
l2dbus_serviceObjectFreeObject
    (
    void*   item,
    void*   userdata
    )
{
    l2dbus_Interface* intfUd = (l2dbus_Interface*)item;
    l2dbus_ServiceObject* svcObjUd = (l2dbus_ServiceObject*)userdata;

    if ( !cdbus_objectRemoveInterface(svcObjUd->obj, cdbus_interfaceGetName(intfUd->intf)) )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Failed to removed interface '%s' from service object",
            cdbus_interfaceGetName(intfUd->intf)));
    }
}


/**
 @function new

 Creates a new ServiceObject.

 Creates a new service object used to implement a D-Bus object that can
 respond to requests, emit signals, and expose properties. The service object
 can be associated a handler that can process client requests. The signature of
 the handler has the form:

     DBusHandlerResult function onRequest(svcObj, conn, msg, userToken)

 Where:

 <ul>
 <li>*svcObj*     - The D-Bus service object</li>
 <li>*conn*       - The D-Bus connection from which the request was received</li>
 <li>*msg*        - The D-Bus request message</li>
 <li>*userToken*  - A value specified by the user when the object was created.</li>
 </ul>

 The handler function should return one of the following values:

 <ul>
 <li>@{l2dbus.Dbus.HANDLER_RESULT_HANDLED|HANDLER_RESULT_HANDLED} - Request handled</li>
 <li>@{l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED|HANDLER_RESULT_NOT_YET_HANDLED} - Request **not** handled</li>
 <li>@{l2dbus.Dbus.HANDLER_RESULT_NEED_MEMORY|HANDLER_RESULT_NEED_MEMORY} - Request **not** handled due to lack of memory</li>
 </ul>

 For all cases except @{l2dbus.Dbus.HANDLER_RESULT_HANDLED|HANDLER_RESULT_HANDLED}
 the client application will receive an appropriate error message for an
 unhandled request.

 @tparam string objPath A valid D-Bus object path.
 @tparam ?func|nil handler An optional object handler function or **nil** if none desired.
 @tparam ?any userToken Optional client data associated with the handler. Will be passed
 to the handler when its invoked.
 @treturn userdata The userdata object representing the ServiceObject.
 */
static int
l2dbus_newServiceObject
    (
    lua_State*  L
    )
{
    l2dbus_ServiceObject* svcObjUd;
    const char* path = NULL;
    int funcIdx = L2DBUS_CALLBACK_NOREF_NEEDED;
    int userIdx = L2DBUS_CALLBACK_NOREF_NEEDED;
    int nArgs;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: service object"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    nArgs = lua_gettop(L);

    path = luaL_checkstring(L, 1);
    if ( !l2dbus_validatePath(path) )
    {
        luaL_error(L, "invalid D-Bus object path");
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

    svcObjUd = (l2dbus_ServiceObject*)l2dbus_objectNew(L, sizeof(*svcObjUd),
                                             L2DBUS_SERVICE_OBJECT_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Service object userdata=%p", svcObjUd));

    if ( NULL == svcObjUd )
    {
        luaL_error(L, "Failed to create service object userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        l2dbus_callbackInit(&svcObjUd->cbCtx);
        l2dbus_refListInit(&svcObjUd->interfaces);

        l2dbus_callbackRef(L, funcIdx, userIdx, &svcObjUd->cbCtx);
        svcObjUd->obj = cdbus_objectNew(path, l2dbus_serviceObjectHandler, svcObjUd);

        if ( NULL == svcObjUd->obj )
        {
            /* Release any references we may still have */
            l2dbus_callbackUnref(L, &svcObjUd->cbCtx);
            l2dbus_refListFree(&svcObjUd->interfaces, L, NULL, NULL);
            luaL_error(L, "Failed to allocate service object");
        }
        else
        {
            /* Create a (weak) mapping between the CDBUS object pointer and
             * the Lua userdata wrapper.
             */
            l2dbus_objectRegistryAdd(L, svcObjUd->obj, -1);
        }
    }

    return 1;
}


/**
 * @brief Called by Lua VM to GC/reclaim the ServiceObject userdata.
 *
 * This method is called by the Lua VM to reclaim the ServiceObject
 * userdata.
 *
 * @return nil
 *
 */
static int
l2dbus_serviceObjectDispose
    (
    lua_State*  L
    )
{
    l2dbus_ServiceObject* ud = (l2dbus_ServiceObject*)luaL_checkudata(L, -1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: service object (userdata=%p)", ud));

    l2dbus_refListFree(&ud->interfaces, L, l2dbus_serviceObjectFreeObject, ud);

    if ( ud->obj != NULL )
    {
        /* Remove the weak association from CDBUS object to the Lua
         * userdata object wrapper.
         */
        l2dbus_objectRegistryRemove(L, ud->obj);
        cdbus_objectUnref(ud->obj);
    }

    /* Unreference the function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


/**
 * The L2DBUS ServiceObject class.
 * @type ServiceObject
 */

/**
 @function path
 @within ServiceObject

 Returns the object path associated with the object.

 @tparam userdata object The userdata representing the ServiceObject.
 @treturn ?string|nil A string with the object path or **nil** if it
 is unset.
 */
static int
l2dbus_serviceObjectGetPath
    (
    lua_State*  L
    )
{
    const char* path;
    l2dbus_ServiceObject* ud = (l2dbus_ServiceObject*)luaL_checkudata(L, 1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    path = cdbus_objectGetPath(ud->obj);
    if ( NULL != path )
    {
        lua_pushstring(L, path);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}


/**
 @function setData
 @within ServiceObject

 Sets the user data to be passed back in the object request handler.

 @tparam userdata object The userdata representing the ServiceObject.
 @tparam any userToken A value to pass back in the object request handler callback.
 */
static int
l2dbus_serviceObjectSetData
    (
    lua_State*  L
    )
{
    l2dbus_ServiceObject* ud = (l2dbus_ServiceObject*)luaL_checkudata(L, 1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);

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
 @function data
 @within ServiceObject

 Retrieves the data that will be passed to the object request handler.

 @tparam userdata object The userdata representing the ServiceObject.
 @treturn any The value that will be returned in the object request handler
 callback.
 */
static int
l2dbus_serviceObjectGetData
    (
    lua_State*  L
    )
{
    l2dbus_ServiceObject* ud = (l2dbus_ServiceObject*)luaL_checkudata(L, 1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

    return 1;
}


/**
 @function addInterface
 @within ServiceObject

 Adds an @{l2dbus.Interface|interface} to the service object.

 A service object can implement more than one interface but the interface
 names must be unique (e.g. no two interfaces with the same name). This
 method will fail if you try to add the same interface twice.

 @tparam userdata object The userdata representing the ServiceObject.
 @tparam userdata interface The @{l2dbus.Interface|interface} to add to
 the object.
 @treturn bool Returns **true** if the interface was added successfully or
 **false** otherwise.
 */
static int
l2dbus_serviceObjectAddInterface
    (
    lua_State*  L
    )
{
    l2dbus_Bool isAdded = L2DBUS_FALSE;

    l2dbus_ServiceObject* objUd = (l2dbus_ServiceObject*)luaL_checkudata(L, 1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);
    l2dbus_Interface* ifUd = (l2dbus_Interface*)luaL_checkudata(L, 2,
                                        L2DBUS_INTERFACE_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    if ( cdbus_objectAddInterface(objUd->obj, ifUd->intf) )
    {
        /* Add a strong reference to the Lua userdata wrapping
         *  the D-Bus interface
         */
        if ( LUA_NOREF == l2dbus_refListRef(&objUd->interfaces, L, 2) )
        {
            /* Failed adding the reference - remove the interface */
            cdbus_objectRemoveInterface(objUd->obj, cdbus_interfaceGetName(ifUd->intf));
        }
        else
        {
            isAdded = L2DBUS_TRUE;
        }
    }

    lua_pushboolean(L, isAdded);

    return 1;
}


/**
 @function removeInterface
 @within ServiceObject

 Removes an @{l2dbus.Interface|interface} from the service object.

 The same interface instance that was originally @{addInterface|added} must be
 used to remove it since the framework keeps a strong reference to that
 interface until it is removed. If the same instance is not used (e.g.
 different interface instance with the same name) then the Lua VM will never
 be able to garbage collect the original interface.

 @tparam userdata object The userdata representing the ServiceObject.
 @tparam userdata interface The @{l2dbus.Interface|interface} to remove from
 the object.
 @treturn bool Returns **true** if the interface was removed successfully or
 **false** otherwise.
 */
static int
l2dbus_serviceObjectRemoveInterface
    (
    lua_State*  L
    )
{
    l2dbus_Bool removed = L2DBUS_FALSE;
    l2dbus_RefListIter iter;

    l2dbus_ServiceObject* objUd = (l2dbus_ServiceObject*)luaL_checkudata(L, 1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);
    l2dbus_Interface* intfUd = (l2dbus_Interface*)luaL_checkudata(L, 2,
                                            L2DBUS_INTERFACE_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    if ( !cdbus_objectRemoveInterface(objUd->obj, cdbus_interfaceGetName(intfUd->intf)) )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Failed to removed interface '%s' from service object",
            cdbus_interfaceGetName(intfUd->intf)));
    }
    /* Else interface removed so remove the reference to the Lua wrapper */
    else
    {
        /* We've removed the actual interface owned by the lower-level CDBUS
         * object.
         */
        removed = L2DBUS_TRUE;

        /* Make best effort to remove the strong reference to the interface */

        /* If we can't find the interface to unreference then ... */
        if ( !l2dbus_refListFindItem(&objUd->interfaces, L, intfUd, &iter) )
        {
            L2DBUS_TRACE((L2DBUS_TRC_ERROR,
                        "Failed to drop reference to interface '%s'",
                        cdbus_interfaceGetName(intfUd->intf)));
        }
        /* Else the interface was found so unreference it and erase element */
        else
        {
            l2dbus_refListIterErase(&objUd->interfaces, L, &iter);
        }
    }

    lua_pushboolean(L, removed);
    return 1;
}


/**
 @function introspect
 @within ServiceObject

 Introspects the object and returns the D-Bus XML introspection data.

 This method generates the D-Bus introspection XML data for a service object
 and returns it as a string. Each interface associated with this object
 will be introspected as well.

 @tparam userdata object The userdata representing the ServiceObject.
 @tparam userdata conn The userdata representing the Connection.
 @treturn ?string|nil Returns the D-Bus XML introspection data or **nil**
 if it is unavailable.
 */
static int
l2dbus_serviceObjectIntrospect
    (
    lua_State*  L
    )
{
    l2dbus_ServiceObject* objUd = (l2dbus_ServiceObject*)luaL_checkudata(L, 1,
                                        L2DBUS_SERVICE_OBJECT_MTBL_NAME);
    l2dbus_Connection* connUd = (l2dbus_Connection*)luaL_checkudata(L, 2,
                                        L2DBUS_CONNECTION_MTBL_NAME);
    const char* path;
    cdbus_StringBuffer* buf;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    path = luaL_checkstring(L, 3);

    buf = cdbus_objectIntrospect(objUd->obj, connUd->conn, path);
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
 * Define the methods of the ServiceObject class
 */
static const luaL_Reg l2dbus_serviceObjectMetaTable[] = {
    {"path", l2dbus_serviceObjectGetPath},
    {"setData", l2dbus_serviceObjectSetData},
    {"data", l2dbus_serviceObjectGetData},
    {"addInterface", l2dbus_serviceObjectAddInterface},
    {"removeInterface", l2dbus_serviceObjectRemoveInterface},
    {"introspect", l2dbus_serviceObjectIntrospect},
    {"__gc", l2dbus_serviceObjectDispose},
    {NULL, NULL},
};


/**
 * @brief Creates the ServiceObject sub-module.
 *
 * This function creates a metatable entry for the ServiceObject userdata
 * and simulates opening the ServiceObject sub-module.
 *
 * @return A table defining the ServiceObject sub-module.
 *
 */
void
l2dbus_openServiceObject
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_SERVICE_OBJECT_TYPE_ID,
        l2dbus_serviceObjectMetaTable));
    lua_newtable(L);
    lua_pushcfunction(L, l2dbus_newServiceObject);
    lua_setfield(L, -2, "new");
}



