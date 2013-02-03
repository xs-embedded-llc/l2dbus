/*******************************************************************************
 *
 * Project         l2dbus
 * (c) Copyright   2012 XS-Embedded LLC
 *                 All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************
 *******************************************************************************
 * @file           l2dbus_serviceobject.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus service object
 *******************************************************************************
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

    /* Nil or the Service Object userdata is sitting at the top of the
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
            l2dbus_messageWrap(L, msg, L2DBUS_FALSE);

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
        /* Else the interface was found so unreference and erase element */
        else
        {
            l2dbus_refListIterErase(&objUd->interfaces, L, &iter);
        }
    }

    lua_pushboolean(L, removed);
    return 1;
}


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



