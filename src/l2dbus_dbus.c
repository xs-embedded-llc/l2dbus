/*===========================================================================
 *
 * Project         l2dbus
 * (c) Copyright   2013 XS-Embedded LLC
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
 * @file           l2dbus_dbus.c        
 * @author         Glenn Schmottlach
 * @brief          Exports shared D-Bus definitions and enumerations.
 *===========================================================================
 */
#include <stdlib.h>
#include <assert.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_dbus.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"

/**
 L2DBUS Shared D-Bus constants

 This module defines shared D-Bus constants that are typically utilized by
 related L2DBUS sub-modules.

 These constants directly correspond to their 'C' D-Bus reference library
 <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusShared.html">equivalents</a>
 and protocol <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusProtocol.html">definitions</a>.
 @module l2dbus.Dbus
 */

const char L2DBUS_DBUS_MTBL_NAME[] = "l2dbus.dbus";

#define L2DBUS_DBUS_STRING_CONST(V) \
    do { \
    lua_pushliteral(L, #V); \
    lua_pushliteral(L, DBUS_##V); \
    lua_rawset(L, -3); \
    } while (0)

#define L2DBUS_DBUS_INT_CONST(V) \
    do { \
    lua_pushliteral(L, #V); \
    lua_pushinteger(L, DBUS_##V); \
    lua_rawset(L, -3); \
    } while (0)

/**
 * @brief Called by Lua VM to GC/reclaim the D-Bus userdata.
 *
 * This method is called by the Lua VM to reclaim the D-Bus
 * userdata.
 *
 * @return nil
 *
 */
static int
l2dbus_dbusDispose
    (
    lua_State*  L
    )
{
    /* There is nothing that needs to be freed */
    return 0;
}


/*
 * Define the methods of the D-Bus
 */
static const luaL_Reg l2dbus_dbusMetaTable[] = {
    {"__gc", l2dbus_dbusDispose},
    {NULL, NULL},
};


/**
 * @brief Registers the D-Bus metatable type.
 *
 * @return The D-Bus metatable on the top of the Lua stack.
 *
 */
int
l2dbus_createDbusMetatable
    (
    lua_State* L
    )
{
    if ( luaL_newmetatable(L, L2DBUS_DBUS_MTBL_NAME) )
    {
        /* Assign the methods to this new metatable */
        luaL_setfuncs(L, l2dbus_dbusMetaTable, 0);

        /* Set it's metatable to point to itself */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }

    return 1;
}


/**
 * @brief Creates the D-Bus sub-module.
 *
 * This function creates a metatable entry for the D-Bus userdata
 * and simulates opening the D-Bus sub-module.
 *
 * @return nil
 *
 */
void
l2dbus_openDbus
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createDbusMetatable(L));

    /* Create a new table to hold all the constants and defines */
    lua_newtable(L);

    /*
     * Initialize the shared constant strings
     */

/**
 @constant SERVICE_DBUS
 The bus name used to talk to the bus itself.
 */
    L2DBUS_DBUS_STRING_CONST(SERVICE_DBUS);
/**
 @constant PATH_DBUS
 The object path used to talk to the bus itself.
 */
    L2DBUS_DBUS_STRING_CONST(PATH_DBUS);
/**
 @constant PATH_LOCAL
 The object path used in local/in-process-generated messages.
 */
    L2DBUS_DBUS_STRING_CONST(PATH_LOCAL);
/**
 @constant INTERFACE_DBUS
 The interface exported by the object with @{SERVICE_DBUS} and @{PATH_DBUS}.
 */
    L2DBUS_DBUS_STRING_CONST(INTERFACE_DBUS);
/**
 @constant INTERFACE_INTROSPECTABLE
 The interface supported by introspectable objects.
 */
    L2DBUS_DBUS_STRING_CONST(INTERFACE_INTROSPECTABLE);
/**
 @constant INTERFACE_PROPERTIES
 The interface supported by objects with properties.
 */
    L2DBUS_DBUS_STRING_CONST(INTERFACE_PROPERTIES);
/**
 @constant INTERFACE_PEER
 The interface supported by most dbus peers.
 */
    L2DBUS_DBUS_STRING_CONST(INTERFACE_PEER);
/**
 @constant INTERFACE_LOCAL
 This is a special interface whose methods can only be invoked by
 the local implementation (messages from remote apps aren't allowed
 to specify this interface).
 */
    L2DBUS_DBUS_STRING_CONST(INTERFACE_LOCAL);

    /*
     * Initialize the shared constants integral values
     */

/**
 @constant NAME_FLAG_ALLOW_REPLACEMENT
 Allow another service to become the primary owner if requested.
 */
    L2DBUS_DBUS_INT_CONST(NAME_FLAG_ALLOW_REPLACEMENT);
/**
 @constant NAME_FLAG_REPLACE_EXISTING
 Request to replace the current primary owner.
 */
    L2DBUS_DBUS_INT_CONST(NAME_FLAG_REPLACE_EXISTING);
/**
 @constant NAME_FLAG_DO_NOT_QUEUE
 If we can not become the primary owner do not place us in the queue.
 */
    L2DBUS_DBUS_INT_CONST(NAME_FLAG_DO_NOT_QUEUE);

/**
 @constant REQUEST_NAME_REPLY_PRIMARY_OWNER
 Service has become the primary owner of the requested name.
 */
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_PRIMARY_OWNER);

/**
 @constant REQUEST_NAME_REPLY_IN_QUEUE
 Service could not become the primary owner and has been placed in the queue.
 */
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_IN_QUEUE);
/**
 @constant REQUEST_NAME_REPLY_EXISTS
 Service is already in the queue.
 */
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_EXISTS);
/**
 @constant REQUEST_NAME_REPLY_ALREADY_OWNER
 Service is already the primary owner.
 */
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_ALREADY_OWNER);
/**
 @constant RELEASE_NAME_REPLY_RELEASED
 Service was released from the given name.
 */
    L2DBUS_DBUS_INT_CONST(RELEASE_NAME_REPLY_RELEASED);
/**
 @constant RELEASE_NAME_REPLY_NON_EXISTENT
 The given name does not exist on the bus.
 */
    L2DBUS_DBUS_INT_CONST(RELEASE_NAME_REPLY_NON_EXISTENT);
/**
 @constant RELEASE_NAME_REPLY_NOT_OWNER
 Service is not an owner of the given name.
 */
    L2DBUS_DBUS_INT_CONST(RELEASE_NAME_REPLY_NOT_OWNER);
/**
 @constant START_REPLY_SUCCESS
 Service was auto started.
 */
    L2DBUS_DBUS_INT_CONST(START_REPLY_SUCCESS);
/**
 @constant START_REPLY_ALREADY_RUNNING
 Service was already running.
 */
    L2DBUS_DBUS_INT_CONST(START_REPLY_ALREADY_RUNNING);
/**
 @constant TIMEOUT_USE_DEFAULT
 Use the default D-Bus timeout value for waiting for a reply.
 */
    L2DBUS_DBUS_INT_CONST(TIMEOUT_USE_DEFAULT);
/**
 @constant TIMEOUT_INFINITE
 Use an infinite timeout waiting for a reply.
 */
    L2DBUS_DBUS_INT_CONST(TIMEOUT_INFINITE);

    /*
     * Initialize the different bus types
     */

/**
 @constant BUS_SESSION
 The *SESSION* D-bus bus.
 */
    L2DBUS_DBUS_INT_CONST(BUS_SESSION);
/**
 @constant BUS_SYSTEM
 The *SYSTEM* D-bus bus.
 */
    L2DBUS_DBUS_INT_CONST(BUS_SYSTEM);
/**
 @constant BUS_STARTER
 The *STARTER* D-bus bus.
 */
    L2DBUS_DBUS_INT_CONST(BUS_STARTER);

    /*
     * Initialize the basic D-Bus message types
     */

/**
 @messageType MESSAGE_TYPE_INVALID
 This value is never a valid message type.
 */
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_INVALID);
/**
 @messageType MESSAGE_TYPE_METHOD_CALL
 Message type of a method call message.
 */
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_METHOD_CALL);
/**
 @messageType MESSAGE_TYPE_METHOD_RETURN
 Message type of a method return message.
 */
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_METHOD_RETURN);
/**
 @messageType MESSAGE_TYPE_ERROR
 Message type of an error reply message.
 */
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_ERROR);
/**
 @messageType MESSAGE_TYPE_SIGNAL
 Message type of a signal message.
 */
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_SIGNAL);

    /*
     * Initialize the D-Bus handler result codes
     */

/**
 @handlerReturn HANDLER_RESULT_HANDLED
 The result to return in a message callback when a message has been handled.
 */
    L2DBUS_DBUS_INT_CONST(HANDLER_RESULT_HANDLED);
/**
 @handlerReturn HANDLER_RESULT_NOT_YET_HANDLED
 The result to return in a message callback when a message **cannot** be handled.
 */
    L2DBUS_DBUS_INT_CONST(HANDLER_RESULT_NOT_YET_HANDLED);
/**
 @handlerReturn HANDLER_RESULT_NEED_MEMORY
 The result to return in a message callback when more memory is needed to process the message.
 */
    L2DBUS_DBUS_INT_CONST(HANDLER_RESULT_NEED_MEMORY);

    /*
     * Initialize the common D-Bus errors
     */

/**
 @dbusError ERROR_FAILED
 A generic error; "something went wrong" - see the error message for more.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_FAILED);
/**
 @dbusError ERROR_NO_MEMORY
 There was not enough memory to complete an operation.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_NO_MEMORY);
/**
 @dbusError ERROR_SERVICE_UNKNOWN
 The bus doesn't know how to launch a service to supply the bus name you wanted.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SERVICE_UNKNOWN);
/**
 @dbusError ERROR_NAME_HAS_NO_OWNER
 The bus name you referenced doesn't exist.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_NAME_HAS_NO_OWNER);
/**
 @dbusError ERROR_NO_REPLY
 No reply to a message expecting one, usually means a timeout occurred.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_NO_REPLY);
/**
 @dbusError ERROR_IO_ERROR
 Something went wrong reading or writing to a socket, for example.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_IO_ERROR);
/**
 @dbusError ERROR_BAD_ADDRESS
 A D-Bus bus address was malformed.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_BAD_ADDRESS);
/**
 @dbusError ERROR_NOT_SUPPORTED
 Requested operation isn't supported (like ENOSYS on UNIX).
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_NOT_SUPPORTED);
/**
 @dbusError ERROR_LIMITS_EXCEEDED
 Some limited resource is exhausted.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_LIMITS_EXCEEDED);
/**
 @dbusError ERROR_ACCESS_DENIED
 Security restrictions don't allow doing what you're trying to do.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_ACCESS_DENIED);
/**
 @dbusError ERROR_AUTH_FAILED
 Authentication didn't work.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_AUTH_FAILED);
/**
 @dbusError ERROR_NO_SERVER
 Unable to connect to server (probably caused by ECONNREFUSED on a socket).
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_NO_SERVER);
/**
 @dbusError ERROR_TIMEOUT
 Certain timeout errors, possibly ETIMEDOUT on a socket.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_TIMEOUT);
/**
 @dbusError ERROR_NO_NETWORK
 No network access (probably ENETUNREACH on a socket).
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_NO_NETWORK);
/**
 @dbusError ERROR_ADDRESS_IN_USE
 Can't bind a socket since its address is in use.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_ADDRESS_IN_USE);
/**
 @dbusError ERROR_DISCONNECTED
 The connection is disconnected and you're trying to use it.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_DISCONNECTED);
/**
 @dbusError ERROR_INVALID_ARGS
 Invalid arguments passed to a method call.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_INVALID_ARGS);
/**
 @dbusError ERROR_FILE_NOT_FOUND
 Missing file.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_FILE_NOT_FOUND);
/**
 @dbusError ERROR_FILE_EXISTS
 Existing file and the operation you're using does not silently overwrite.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_FILE_EXISTS);
/**
 @dbusError ERROR_UNKNOWN_METHOD
 Method name you invoked isn't known by the object you invoked it on.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_UNKNOWN_METHOD);
/**
 @dbusError ERROR_UNKNOWN_OBJECT
 Object you invoked a method on isn't known.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_UNKNOWN_OBJECT);
/**
 @dbusError ERROR_UNKNOWN_INTERFACE
 Interface you invoked a method on isn't known by the object.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_UNKNOWN_INTERFACE);
/**
 @dbusError ERROR_UNKNOWN_PROPERTY
 Property you tried to access isn't known by the object.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_UNKNOWN_PROPERTY);
/**
 @dbusError ERROR_PROPERTY_READ_ONLY
 Property you tried to set is read-only.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_PROPERTY_READ_ONLY);
/**
 @dbusError ERROR_TIMED_OUT
 Certain timeout errors.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_TIMED_OUT);
/**
 @dbusError ERROR_MATCH_RULE_NOT_FOUND
 Tried to remove or modify a match rule that didn't exist.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_MATCH_RULE_NOT_FOUND);
/**
 @dbusError ERROR_MATCH_RULE_INVALID
 The match rule isn't syntactically valid.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_MATCH_RULE_INVALID);
/**
 @dbusError ERROR_SPAWN_EXEC_FAILED
 While starting a new process, the exec() call failed.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_EXEC_FAILED);
/**
 @dbusError ERROR_SPAWN_FORK_FAILED
 While starting a new process, the fork() call failed.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_FORK_FAILED);
/**
 @dbusError ERROR_SPAWN_CHILD_EXITED
 While starting a new process, the child exited with a status code.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_CHILD_EXITED);
/**
 @dbusError ERROR_SPAWN_CHILD_SIGNALED
 While starting a new process, the child exited on a signal.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_CHILD_SIGNALED);
/**
 @dbusError ERROR_SPAWN_FAILED
 While starting a new process, something went wrong.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_FAILED);
/**
 @dbusError ERROR_SPAWN_SETUP_FAILED
 We failed to setup the environment correctly.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_SETUP_FAILED);
/**
 @dbusError ERROR_SPAWN_CONFIG_INVALID
 We failed to setup the config parser correctly.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_CONFIG_INVALID);
/**
 @dbusError ERROR_SPAWN_SERVICE_INVALID
 Bus name was not valid.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_SERVICE_INVALID);
/**
 @dbusError ERROR_SPAWN_SERVICE_NOT_FOUND
 Service file not found in system-services directory.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_SERVICE_NOT_FOUND);
/**
 @dbusError ERROR_SPAWN_PERMISSIONS_INVALID
 Permissions are incorrect on the setuid helper.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_PERMISSIONS_INVALID);
/**
 @dbusError ERROR_SPAWN_FILE_INVALID
 Service file invalid (Name, User or Exec missing).
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_FILE_INVALID);
/**
 @dbusError ERROR_SPAWN_NO_MEMORY
 Tried to get a UNIX process ID and it wasn't available.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SPAWN_NO_MEMORY);
/**
 @dbusError ERROR_UNIX_PROCESS_ID_UNKNOWN
 Tried to get a UNIX process ID and it wasn't available.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_UNIX_PROCESS_ID_UNKNOWN);
/**
 @dbusError ERROR_INVALID_SIGNATURE
 A type signature is not valid.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_INVALID_SIGNATURE);
/**
 @dbusError ERROR_INVALID_FILE_CONTENT
 A file contains invalid syntax or is otherwise broken.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_INVALID_FILE_CONTENT);
/**
 @dbusError ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN
 Asked for SELinux security context and it wasn't available.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN);
/**
 @dbusError ERROR_ADT_AUDIT_DATA_UNKNOWN
 Asked for ADT audit data and it wasn't available.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_ADT_AUDIT_DATA_UNKNOWN);
/**
 @dbusError ERROR_OBJECT_PATH_IN_USE
 There's already an object with the requested object path.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_OBJECT_PATH_IN_USE);
/**
 @dbusError ERROR_INCONSISTENT_MESSAGE
 The message meta data does not match the payload.
 */
    L2DBUS_DBUS_STRING_CONST(ERROR_INCONSISTENT_MESSAGE);
}


