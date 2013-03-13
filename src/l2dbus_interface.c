/*===========================================================================
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
 receive and handle client requests or the associated service object can
 handle these as well as a fallback.


 @module l2dbus.Interface
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
 * @brief Handles and processes requests to the interface.
 *
 * This function will try to deliver a callback to a Lua handler function
 * when invoked. The handler itself will determine whether or not it
 * can handle the request.
 *
 * @param [in] conn     The CDBUS connection associated with this interface
 * request.
 * @param [in] obj      The CDBUS service object implementing the interface.
 * @param [in] msg      The D-Bus request message (e.g. method call).
 * @param [in] userdata Opaque data provided by the client when the handler
 * was registered.
 * @return A suitable DBusHandlerResult value. Returning
 * DBUS_HANDLER_RESULT_HANDLED indicates the interface handler processed
 * the request. Otherwise the service object will be given a change to
 * handle the request.
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

    /* Nil or the Service Object userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != ud );
    assert( NULL != L );

    /* If the watch userdata has been GC'ed then ... */
    if ( NULL == ud )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Cannot call interface handler because service object has been GC'ed"));
    }
    else if ( LUA_NOREF != ud->cbCtx.funcRef )
    {
        /* Push function and user value on the stack and execute the callback */
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.funcRef);
        /* Push the service object userdata */
        lua_pushvalue(L, -2 /* Service object ud */);
        /* Push the associated Lua userdata wrapper on the stack */
        l2dbus_objectRegistryGet(L, conn);
        if ( lua_isnil(L, -1) )
        {
            L2DBUS_TRACE((L2DBUS_TRC_WARN,
                        "Cannot call interface handler because connection has been GC'ed"));
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
                L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Interface callback error: %s", errMsg));
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

     DBusHandlerResult function onRequest(svcObj, conn, msg, userToken)

 Where:

 <ul>
 <li>*svcObj*     - The D-Bus service object implementing this interface</li>
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

 For all cases except @{l2dbus.Dbus.HANDLER_RESULT_HANDLED|HANDLER_RESULT_HANDLED} if the
 service object has a handler then it will be given the opportunity to handle the request.
 If there are no handlers to satisfy the request or an error occurs the client will
 receive an appropriate error message in reply.

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
    size_t itemIdx;
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


static const luaL_Reg l2dbus_interfaceMetaTable[] = {
    {"path", l2dbus_interfaceGetName},
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



