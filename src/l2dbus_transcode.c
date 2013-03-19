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
 * @file           l2dbus_transcode.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus/Lua type transcoding routines
 *===========================================================================
 */
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <luaconf.h>
#include "cdbus/cdbus.h"
#include "l2dbus_transcode.h"
#include "l2dbus_types.h"
#include "l2dbus_compat.h"
#include "l2dbus_object.h"
#include "l2dbus_int64.h"
#include "l2dbus_uint64.h"
#include "l2dbus_util.h"
#include "l2dbus_defs.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_alloc.h"

/**
 The L2DBUS DbusTypes module.

 This module provides wrapper classes for the D-Bus types. The D-Bus type
 wrappers provide a *hint* to the marshalling code that converts between
 native Lua types and D-Bus types. Without any hint or a D-Bus introspective
 signature a heuristic approach is used to convert between the two type systems.
 Most of the time these conversions will be correct but the Lua/D-Bus wrapper
 types and message signatures help avoid potential conversion ambiguity.
 @module l2dbus.DbusTypes
 */


#define L2DBUS_DEFAULT_SIGNATURE_LENGTH (32)

#ifdef LUA_NUMBER_DOUBLE
#define L2DBUS_LUA_MANTISSA_DIG (DBL_MANT_DIG)
#elif defined(LUA_NUMBER_FLOAT)
#define L2DBUS_LUA_MANTISSA_DIG (FLT_MANT_DIG)
#else
#define L2DBUS_LUA_MANTISSA_DIG (sizeof(LUA_NUMBER) * 8)
#endif

/*
 * Define the min/max values that can be expressed as an integer without
 * losing precision.
 */
#define L2DBUS_MAX_INTEGRAL_LUA_NUM ((2LL << (L2DBUS_LUA_MANTISSA_DIG - 1)) - 1)
#define L2DBUS_MIN_INTEGRAL_LUA_NUM (-(2LL << (L2DBUS_LUA_MANTISSA_DIG - 1)))

/*
 * Forward prototypes
 */
static l2dbus_Bool l2dbus_dbusIsTableArray(lua_State* L, int idx);
static l2dbus_Bool l2dbus_dbusIsTableDictionary(lua_State* L, int idx);
static l2dbus_Bool l2dbus_dbusIsTableStructure(lua_State* L, int idx);
static int l2dbus_transcodeMapLuaToDbusType(lua_State* L, int idx);
static void l2dbus_transcodeMarshallAsType(lua_State* L, int argIdx,
                                DBusMessageIter* msgIt, DBusSignatureIter* sigIt);


/**
 * @brief Called to create a Lua userdata proxy for a D-Bus type.
 *
 * This method create a custom Lua userdata type that contains the metadata
 * necessary to describe an associated D-Bus type so that the conversion
 * from the Lua type to D-Bus type is simple.
 *
 * @param [in]  L           The Lua state
 * @param [in]  metaTypeId  The L2DBUS type identifier associated with this
 * userdata.
 * @param [in]  signature   An (optional) D-Bus signature to associate with
 * this type. This is typically assigned for the more complex D-Bus types. It
 * can be NULL if no signature is provided.
 *
 * @return A pointer to the Lua userdata with the actually userdata left at
 * the top of the Lua stack.
 */
static l2dbus_DbusValue*
l2dbus_dbusNewUserdata
    (
        lua_State*  L,
        l2dbus_TypeId metaTypeId,
        const char* signature
    )
{
    char* sigCopy = NULL;
    l2dbus_DbusValue* ud;
    DBusSignatureIter sigIt;
    DBusSignatureIter sigSubIt;
    int dbusType;

    if ( NULL != signature )
    {
        if ( !dbus_signature_validate(signature, NULL) )
        {
            luaL_error(L, "invalid signature");
        }

        dbus_signature_iter_init(&sigIt, signature);
        dbusType = dbus_signature_iter_get_current_type(&sigIt);
        switch ( metaTypeId )
        {
            case L2DBUS_DBUS_ARRAY_TYPE_ID:
                luaL_argcheck(L, DBUS_TYPE_ARRAY == dbusType, 2,
                    "signature does not describe a D-Bus array");
                break;

            case L2DBUS_DBUS_STRUCT_TYPE_ID:
                luaL_argcheck(L, DBUS_TYPE_STRUCT == dbusType, 2,
                    "signature does not describe a D-Bus structure");
                break;

            case L2DBUS_DBUS_VARIANT_TYPE_ID:
                luaL_argcheck(L, DBUS_TYPE_VARIANT == dbusType, 2,
                    "signature does not describe a D-Bus variant");
                break;

            case L2DBUS_DBUS_DICT_ENTRY_TYPE_ID:
                luaL_argcheck(L, DBUS_TYPE_ARRAY == dbusType, 2,
                    "signature does not describe a D-Bus dictionary");
                dbus_signature_iter_recurse(&sigIt, &sigSubIt);
                dbusType = dbus_signature_iter_get_current_type(&sigSubIt);
                luaL_argcheck(L, DBUS_TYPE_DICT_ENTRY == dbusType, 2,
                     "signature does not describe a D-Bus dictionary");
                break;

            default:
                break;
        }

        sigCopy = l2dbus_strDup(signature);
        if ( NULL == sigCopy )
        {
            luaL_error(L, "failed to allocate memory for signature");
        }
    }

    ud = (l2dbus_DbusValue*)l2dbus_objectNew(L, sizeof(*ud), metaTypeId);
    ud->signature = sigCopy;

    return ud;
}


/**
 * @brief Gets a cached D-Bus signature if it's available.
 *
 * This method attempts to retrieve a cached D-Bus signature from a Lua
 * userdata type that wraps a value to be converted to a specific D-Bus
 * type.
 *
 * @param [in]  L       The Lua state
 * @param [in]  argIdx  The index on the Lua stack where the userdata is
 * located.
 *
 * @return A pointer to the cached D-Bus signature or NULL if none exists.
 */
static const char*
l2dbus_dbusGetCachedSignature
    (
    lua_State*  L,
    int         argIdx
    )
{
    const char* signature = NULL;
    argIdx = lua_absindex(L, argIdx);
    l2dbus_DbusValue* ud;

    ud = l2dbus_isUserData(L, argIdx, l2dbus_getTypeName(L, argIdx));
    if ( NULL != ud )
    {
        signature = ud->signature;
    }

    return signature;
}


/**
 * @brief Attaches a Lua value to a Lua D-Bus userdata wrapper.
 *
 * This method associates any Lua value to a Lua userdata type that is
 * being used to provide metadata information for an associated D-Bus type.
 * The value is anchored in a table that is associated with the userdata
 * type.
 *
 * @param [in]      L           The Lua state.
 * @param [in,out]  ud          Pointer to a structure that will be initialized
 * to reference the stored value value.
 * @param [in]      udIdx       The reference to the Lua stack location where
 * the Lua userdata wrapper is stored.
 * @param [in]      valudIdx    The reference to the Lua stack location where
 * the value is stored.
 *
 * @return The Lua stack remains the same on exit as it did on entry. Returns
 * one (1) as a convenience to the constructor functions that call this.
 */
static int
l2dbus_dbusAttachValue
    (
    lua_State*          L,
    l2dbus_DbusValue*   ud,
    int                 udIdx,
    int                 valueIdx
    )
{
    udIdx = lua_absindex(L, udIdx);
    valueIdx = lua_absindex(L, valueIdx);

    assert( NULL != ud );

    /* See if a table has already been associated
     * with this userdata.
     */
    lua_getuservalue(L, udIdx);
    if ( LUA_TNIL == lua_type(L, -1) )
    {
        /* No table exists - we need to create a new table
         * to hold the references to the "value".
         */
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, valueIdx);
        ud->valueRef = luaL_ref(L, -2);
        lua_setuservalue(L, udIdx);
    }
    /* Else the userdata already has a table associated with it */
    else
    {
        lua_pushvalue(L, valueIdx);
        ud->valueRef = luaL_ref(L, -2);
        lua_pop(L, 1);
    }
    return 1;
}


/**
 * @brief Gets the D-Bus type ID for the Lua userdata wrapper object.
 *
 * This is a helper function to return the corresponding D-Bus type
 * identifier assigned to the Lua userdata wrapper object.
 *
 * @param [in]      L       The Lua state.
 * @param [in]      idx     The index to the Lua userdata object.
 * @param [in,out]  typeId  Pointer to the value that will be assigned the
 * D-Bus type identifier.
 *
 * @return Return true if the referenced object is a valid D-Bus userdata
 * object, false otherwise.
 */
static l2dbus_Bool
l2dbus_dbusQueryDbusTypeId
    (
    lua_State*  L,
    int         idx,
    int*        typeId
    )
{
    int validType = L2DBUS_FALSE;

    if ( LUA_TUSERDATA == lua_type(L, idx) )
    {
        lua_getfield(L, idx, "__dbusTypeId");
        if ( lua_isnumber(L, -1) )
        {
            switch ( lua_tointeger(L, -1) )
            {
                case DBUS_TYPE_INVALID:
                case DBUS_TYPE_BYTE:
                case DBUS_TYPE_BOOLEAN:
                case DBUS_TYPE_INT16:
                case DBUS_TYPE_UINT16:
                case DBUS_TYPE_INT32:
                case DBUS_TYPE_UINT32:
                case DBUS_TYPE_INT64:
                case DBUS_TYPE_UINT64:
                case DBUS_TYPE_DOUBLE:
                case DBUS_TYPE_STRING:
                case DBUS_TYPE_OBJECT_PATH:
                case DBUS_TYPE_SIGNATURE:
                case DBUS_TYPE_ARRAY:
                case DBUS_TYPE_STRUCT:
                case DBUS_TYPE_VARIANT:
                case DBUS_TYPE_DICT_ENTRY:
                case DBUS_TYPE_UNIX_FD:
                    if ( NULL != typeId )
                    {
                        *typeId = lua_tointeger(L, -1);
                    }
                    validType = L2DBUS_TRUE;
                    break;
                default:
                    break;
            }
        }

        /* Pop the type (or nil) from the stack */
        lua_pop(L, 1);
    }

    return validType;
}


/**
 * @brief Gets the value of the indexed object.
 *
 * This helper function returns the value of the referenced object. If it's
 * a Lua D-Bus userdata object then it's "value" is looked up. For a standard
 * Lua type a copy of the value is just pushed on the top of the Lua stack.
 * The top of the Lua stack will either have the value or 'nil' if nothing can
 * be retrieved.
 *
 * @param [in]      L       The Lua state.
 * @param [in]      idx     The index to the Lua object on the stack.
 *
 * @return Returns with the associated object value or 'nil' on the top of
 * the Lua stack.
 */
static void
l2dbus_transcodeGetValue
    (
    lua_State*  L,
    int         idx
    )
{
    int typeId = DBUS_TYPE_INVALID;
    l2dbus_DbusValue* ud;
    idx = lua_absindex(L, idx);

    if ( l2dbus_dbusQueryDbusTypeId(L, idx, &typeId) )
    {
        ud = lua_touserdata(L, idx);
        lua_getuservalue(L, idx);
        if ( !lua_isnil(L, -1) )
        {
            lua_rawgeti(L, -1, ud->valueRef);
            /* Remove the user value table */
            lua_remove(L, -2);
        }
    }
    /* Else must be a standard Lua type or Int64/Uint64 userdata */
    else
    {
        /* Leave a copy at the top of the stack */
        lua_pushvalue(L, idx);
    }
}


/**
 * @brief Computes the D-Bus signature of the specified argument.
 *
 * This (potentially) recursive function computes the D-Bus signature
 * of the specified Lua argument. Both heuristics and hints are used to
 * complete this conversion from a Lua object to a D-Bus signature.
 *
 * @param [in]      L       The Lua state.
 * @param [in]      argIdx  The index to the argument on the Lua stack.
 * @param [in,out]  sigBuf  The buffer which holds the signature.
 * @param [in]      level   The recursive "level" at which this is being
 * called. Starts a zero (0).
 *
 * @return Returns true if the siguature was computed successfully, false
 * otherwise. The returned signature is in the sigBuf on success.
 */
l2dbus_Bool
l2dbus_dbusComputeSignature
    (
    lua_State*          L,
    int                 argIdx,
    cdbus_StringBuffer* sigBuf,
    int                 level
    )
{
    int origTop = lua_gettop(L);
    int dbusTypeId;
    size_t idx;
    size_t arrayLen;
    const char* sigStr = NULL;
    const char* cachedSig = NULL;
    l2dbus_Bool isValid = L2DBUS_TRUE;

    /* Reset to the absolute index */
    argIdx = lua_absindex(L, argIdx);
    assert( NULL != sigBuf );

    if ( DBUS_MAXIMUM_SIGNATURE_LENGTH <= cdbus_stringBufferLength(sigBuf) )
    {
        /* Argument signature is too large */
        isValid = L2DBUS_FALSE;
    }
    else if ( DBUS_MAXIMUM_TYPE_RECURSION_DEPTH < level )
    {
        /* Maximum signature recursion depth exceeded */
        isValid = L2DBUS_FALSE;
    }
    else
    {
        dbusTypeId = l2dbus_transcodeMapLuaToDbusType(L, argIdx);

        switch ( dbusTypeId )
        {
            case DBUS_TYPE_BYTE:
                sigStr = DBUS_TYPE_BYTE_AS_STRING;
                break;

            case DBUS_TYPE_BOOLEAN:
                sigStr = DBUS_TYPE_BOOLEAN_AS_STRING;
                break;

            case DBUS_TYPE_INT16:
                sigStr = DBUS_TYPE_INT16_AS_STRING;
                break;

            case DBUS_TYPE_UINT16:
                sigStr = DBUS_TYPE_UINT16_AS_STRING;
                break;

            case DBUS_TYPE_INT32:
                sigStr = DBUS_TYPE_INT32_AS_STRING;
                break;

            case DBUS_TYPE_UINT32:
                sigStr = DBUS_TYPE_UINT32_AS_STRING;
                break;

            case DBUS_TYPE_INT64:
                sigStr = DBUS_TYPE_INT64_AS_STRING;
                break;

            case DBUS_TYPE_UINT64:
                sigStr = DBUS_TYPE_UINT64_AS_STRING;
                break;

            case DBUS_TYPE_DOUBLE:
                sigStr = DBUS_TYPE_DOUBLE_AS_STRING;
                break;

            case DBUS_TYPE_STRING:
                sigStr = DBUS_TYPE_STRING_AS_STRING;
                break;

            case DBUS_TYPE_OBJECT_PATH:
                sigStr = DBUS_TYPE_OBJECT_PATH_AS_STRING;
                break;

            case DBUS_TYPE_SIGNATURE:
                sigStr = DBUS_TYPE_SIGNATURE_AS_STRING;
                break;

            case DBUS_TYPE_ARRAY:
                cachedSig = l2dbus_dbusGetCachedSignature(L, argIdx);
                if ( NULL != cachedSig )
                {
                    if ( strlen(cachedSig) !=
                        cdbus_stringBufferAppend(sigBuf, cachedSig) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                }
                else
                {
                    if ( strlen(DBUS_TYPE_ARRAY_AS_STRING) !=
                        cdbus_stringBufferAppend(sigBuf,
                            DBUS_TYPE_ARRAY_AS_STRING) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                    else
                    {
                        /* Push a copy of the actual Lua type that is
                         * associated with the D-Bus value on the stack
                         */
                        l2dbus_transcodeGetValue(L, argIdx);

                        /* This should be a Lua "array" */
                        arrayLen = lua_rawlen(L, -1);
                        if ( 0 == arrayLen )
                        {
                            isValid = L2DBUS_FALSE;
                        }
                        else
                        {
                            /* Use the first element of the array as the
                             * template for the signature of *every* element.
                             * This may not be a great assumption but looping
                             * over every element and validating it incurs a
                             * lot of overhead for the general case.
                             */
                            lua_rawgeti(L, -1, 1);
                            isValid = l2dbus_dbusComputeSignature(L, -1,
                                                            sigBuf, level + 1);
                        }

                        /* Pop off the D-Bus value we pushed earlier */
                        lua_pop(L, 1);
                    }
                }
                break;

            case DBUS_TYPE_STRUCT:
                cachedSig = l2dbus_dbusGetCachedSignature(L, argIdx);
                if ( NULL != cachedSig )
                {
                    if ( strlen(cachedSig) !=
                        cdbus_stringBufferAppend(sigBuf, cachedSig) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                }
                else
                {
                    if ( strlen(DBUS_STRUCT_BEGIN_CHAR_AS_STRING) !=
                        cdbus_stringBufferAppend(sigBuf,
                                DBUS_STRUCT_BEGIN_CHAR_AS_STRING) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                    else
                    {
                        /* Push a copy of the actual Lua type that is
                         * associated with the D-Bus value on the stack
                         */
                        l2dbus_transcodeGetValue(L, argIdx);
                        arrayLen = lua_rawlen(L, -1);

                        /* Loop over every element and encode it */
                        for ( idx = 1; (idx <= arrayLen) && isValid; ++idx )
                        {
                            lua_rawgeti(L, -1, idx);
                            isValid = l2dbus_dbusComputeSignature(L, -1,
                                                            sigBuf, level + 1);
                            lua_pop(L, 1);
                        }

                        if ( isValid )
                        {
                            if ( strlen(DBUS_STRUCT_END_CHAR_AS_STRING) !=
                                cdbus_stringBufferAppend(sigBuf,
                                DBUS_STRUCT_END_CHAR_AS_STRING) )
                            {
                                isValid = L2DBUS_FALSE;
                            }
                        }

                        /* Pop off the D-Bus value we pushed earlier */
                        lua_pop(L, 1);
                    }
                }
                break;

            case DBUS_TYPE_VARIANT:
                cachedSig = l2dbus_dbusGetCachedSignature(L, argIdx);
                if ( NULL != cachedSig )
                {
                    if ( strlen(cachedSig) !=
                        cdbus_stringBufferAppend(sigBuf, cachedSig) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                }
                else
                {
                    if ( strlen(DBUS_TYPE_VARIANT_AS_STRING) !=
                        cdbus_stringBufferAppend(sigBuf,
                        DBUS_TYPE_VARIANT_AS_STRING) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                    else
                    {
                        /* Push a copy of the actual Lua type that is
                         * associated with the D-Bus value on the stack
                         */
                        l2dbus_transcodeGetValue(L, argIdx);
                        isValid = l2dbus_dbusComputeSignature(L, -1, sigBuf,
                                                            level + 1);

                        /* Pop off the D-Bus value we pushed earlier */
                        lua_pop(L, 1);
                    }
                }
                break;

            case DBUS_TYPE_DICT_ENTRY:
                cachedSig = l2dbus_dbusGetCachedSignature(L, argIdx);
                if ( NULL != cachedSig )
                {
                    if ( strlen(cachedSig) !=
                        cdbus_stringBufferAppend(sigBuf, cachedSig) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                }
                else
                {
                    if ( strlen(DBUS_TYPE_ARRAY_AS_STRING
                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING) !=
                        cdbus_stringBufferAppend(sigBuf,
                        DBUS_TYPE_ARRAY_AS_STRING
                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING) )
                    {
                        isValid = L2DBUS_FALSE;
                    }
                    else
                    {
                        /* Push a copy of the actual Lua type that is
                         * associated with the D-Bus value on the stack
                         */
                        l2dbus_transcodeGetValue(L, argIdx);


                        /* The key and value signatures are determined by the
                         * first element of the Lua table.
                         */
                        lua_pushnil(L);
                        while ( lua_next(L, -2) )
                        {
                            /* Compute the key based on the first element */
                            isValid = l2dbus_dbusComputeSignature(L, -2, sigBuf,
                                                                level + 1);
                            if ( isValid )
                            {
                                /* For Lua tables we'll assume the value may
                                 * vary in type. As a result we always consider
                                 * it a variant type unless explicitly
                                 * indicated otherwise.
                                 */
                                if ( strlen(DBUS_TYPE_VARIANT_AS_STRING) !=
                                    cdbus_stringBufferAppend(sigBuf,
                                    DBUS_TYPE_VARIANT_AS_STRING) )
                                {
                                    isValid = L2DBUS_FALSE;
                                }
                            }

                            /* We found our first element - break out of loop */
                            lua_pop(L, 2);
                            break;
                        }

                        if ( isValid )
                        {
                            if ( strlen(DBUS_DICT_ENTRY_END_CHAR_AS_STRING) !=
                                cdbus_stringBufferAppend(sigBuf,
                                DBUS_DICT_ENTRY_END_CHAR_AS_STRING) )
                            {
                                isValid = L2DBUS_FALSE;
                            }
                        }

                        /* Pop off the D-Bus value we pushed earlier */
                        lua_pop(L, 1);
                    }
                }
                break;

            case DBUS_TYPE_UNIX_FD:
                sigStr = DBUS_TYPE_UNIX_FD_AS_STRING;
                break;

            default:
                isValid = L2DBUS_FALSE;
                break;
        }

        if ( isValid )
        {
            if ( ((NULL != sigStr) &&
                (strlen(sigStr) != cdbus_stringBufferAppend(sigBuf, sigStr))) )
            {
                isValid = L2DBUS_FALSE;
            }
        }
    }

    lua_settop(L, origTop);

    return isValid;
}


/**
 * @brief Gets the value of the associated D-Bus Lua userdata object.
 *
 * This function is called directly from Lua to retrieve the "value"
 * associated with the D-Bus userdata object. It is assumed this userdata
 * object sits on the top of the Lua stack.
 *
 * @param [in]      L       The Lua state.
 *
 * @return Returns the Lua value on the top of the Lua stack.
 */
static int
l2dbus_dbusGetLuaValue
    (
    lua_State*  L
    )
{
    l2dbus_transcodeGetValue(L, 1);
    return 1;
}


/**
 * @brief Gets the D-Bus type ID of the associated D-Bus Lua userdata object.
 *
 * This function is called directly from Lua to retrieve the D-Bus type
 * identifier associated with the D-Bus userdata object. It is assumed this
 * userdata object sits on the top of the Lua stack.
 *
 * @param [in]      L       The Lua state.
 *
 * @return Returns the D-Bus type identifier on the top of the Lua stack.
 */
static int
l2dbus_dbusGetTypeId
    (
    lua_State*  L
    )
{
    int typeId = DBUS_TYPE_INVALID;

    luaL_argcheck(L, l2dbus_dbusQueryDbusTypeId(L, 1, &typeId), 1,
                   "Unknown type");
    lua_pushinteger(L, typeId);
    return 1;
}


/**
 * @brief Converts the D-Bus Lua userdata object to a string.
 *
 * This function is called directly from Lua to convert the value associated
 * with the D-Bus Lua userdata to its string representation. It is assumed
 * the userdata object sits on the top of the Lua stack when the function
 * is called by the VM.
 *
 * @param [in]      L       The Lua state.
 *
 * @return Returns the string representation of Lua value associated with
 * the D-Bus Lua userdata. This string is returned on the top of the Lua stack.
 */
static int
l2dbus_dbusToString
    (
    lua_State*  L
    )
{
    l2dbus_transcodeGetValue(L, 1);
    l2dbus_getGlobalField(L, "tostring");
    lua_pushvalue(L, -2);
    lua_call(L, 1, 1);
    return 1;
}


/**
 * @brief Tests to see if a Lua table can be converted to a D-Bus dictionary.
 *
 * This function is used to determine whether a Lua table can be encoded as
 * a D-Bus dictionary (e.g. an array of dictionary entries). A D-Bus dictionary
 * is the least restrictive data structure that D-Bus permits.
 *
 * @param [in]      L       The Lua state.
 * @param [in]      idx     Index on the Lua stack where the Lua table is
 * located.
 *
 * @return Returns true if the table could be converted to a D-Bus dictionary,
 * otherwise false.
 */
static l2dbus_Bool
l2dbus_dbusIsTableDictionary
    (
    lua_State*  L,
    int         idx
    )
{
    int tableIdx = lua_absindex(L, idx);
    l2dbus_Bool isDict = L2DBUS_FALSE;
    int luaType = LUA_TNONE;
    int dbusType = DBUS_TYPE_INVALID;
    l2dbus_TypeId metaTypeId = L2DBUS_INVALID_TYPE_ID;

    if ( LUA_TTABLE == lua_type(L, tableIdx) )
    {
        /* Now we'll assume this is a dictionary */
        isDict = L2DBUS_TRUE;

        lua_pushnil(L);
        while ( lua_next(L, tableIdx) )
        {
            /* First make sure the key is a basic D-Bus compatible
             * type.
             */
            luaType = lua_type(L, -2);
            switch ( luaType )
            {
                case LUA_TNUMBER:
                case LUA_TBOOLEAN:
                case LUA_TSTRING:
                    break;

                case LUA_TUSERDATA:
                    metaTypeId = l2dbus_getMetaTypeId(L, -2);
                    if ( !l2dbus_dbusQueryDbusTypeId(L, -2, &dbusType) )
                    {
                        if ( (L2DBUS_INT64_TYPE_ID != metaTypeId) &&
                            (L2DBUS_UINT64_TYPE_ID != metaTypeId) )
                        {
                            isDict = L2DBUS_FALSE;
                        }
                    }
                    else
                    {
                        /* Only *basic* D-Bus types are acceptable
                         * to be used as keys in a dictionary.
                         */
                        if ( !dbus_type_is_basic(dbusType) )
                        {
                            isDict = L2DBUS_FALSE;
                        }
                    }
                    break;

                default:
                    isDict = L2DBUS_FALSE;
                    break;
            }

            /* If the key checks out then check the "value" */
            if ( isDict )
            {
                /* We need to make sure the values are valid D-Bus types. */
                luaType = lua_type(L, -1);
                switch ( luaType )
                {
                    case LUA_TNUMBER:
                    case LUA_TBOOLEAN:
                    case LUA_TSTRING:
                        break;

                    case LUA_TTABLE:
                        /* A Lua table can either be mapped to a D-Bus
                         * dictionary, structure, or array. Since the criteria
                         * for a D-Bus dictionary is the loosest representation
                         * and if it can be represented as a D-Bus dictionary
                         * then a sufficient mapping exists. It would be
                         * redundant to verify that it can be mapped to other
                         * types.
                         */
                        if ( !l2dbus_dbusIsTableDictionary(L, -1) )
                        {
                            isDict = L2DBUS_FALSE;
                        }
                        break;

                    case LUA_TUSERDATA:
                        metaTypeId = l2dbus_getMetaTypeId(L, -1);
                        /* Make sure it's l2dbus userdata */
                        if ( NULL == l2dbus_isUserData(L, -1,
                            l2dbus_getNameByTypeId(metaTypeId)) )
                        {
                            isDict = L2DBUS_FALSE;
                        }

                        /* Now make sure it's a type of L2DBUS userdata
                         * that can be sent over D-Bus
                         */
                        if ( !(
                            (L2DBUS_INT64_TYPE_ID == metaTypeId) ||
                            (L2DBUS_UINT64_TYPE_ID == metaTypeId) ||
                            ((L2DBUS_START_DBUS_TYPE_ID < metaTypeId) &&
                             (L2DBUS_END_DBUS_TYPE_ID > metaTypeId) &&
                             (L2DBUS_DBUS_INVALID_TYPE_ID != metaTypeId))
                             ) )
                        {
                            isDict = L2DBUS_FALSE;
                        }
                        break;

                    default:
                        isDict = L2DBUS_FALSE;
                        break;
                }
            }

            if ( !isDict )
            {
                /* Pop the key/value pair */
                lua_pop(L, 2);
                break;
            }
            else
            {
                /* Get ready for the next go-around */
                lua_pop(L, 1);
            }
        }
    }

    return isDict;
}


/**
 * @brief Tests to see if a Lua table can be converted to a D-Bus array.
 *
 * This function is used to determine whether a Lua table can be encoded as
 * a D-Bus array. A D-Bus array requires the Lua table to have consecutively
 * number elements (no holes) all of the same homogeneous type.
 *
 * @param [in]      L       The Lua state.
 * @param [in]      idx     Index on the Lua stack where the Lua table is
 * located.
 *
 * @return Returns true if the table could be converted to a D-Bus array,
 * otherwise false.
 */
static l2dbus_Bool
l2dbus_dbusIsTableArray
    (
    lua_State*  L,
    int         idx
    )
{
    size_t itemCnt = 0;
    int tableIdx = lua_absindex(L, idx);
    l2dbus_Bool isArray = L2DBUS_FALSE;
    int luaType = LUA_TNONE;
    l2dbus_TypeId metaTypeId = L2DBUS_INVALID_TYPE_ID;
    l2dbus_TypeId curMetaTypeId = L2DBUS_INVALID_TYPE_ID;

    if ( LUA_TTABLE == lua_type(L, tableIdx) )
    {
        /* Now we'll assume this is a valid array */
        isArray = L2DBUS_TRUE;

        lua_pushnil(L);
        while ( lua_next(L, tableIdx) )
        {
            /* First make sure the key is a number
             */
            if ( !lua_isnumber(L, -2) )
            {
                lua_pop(L, 2);
                isArray = L2DBUS_FALSE;
                break;
            }

            /* Count the number of items in the array */
            ++itemCnt;

            /* Now verify the value is a supported type and
             * the array contains homogeneous values since
             * D-Bus does *not* support a non-homogeneous mixture.
             */

            /* If this is the first element */
            if ( LUA_TNONE == luaType )
            {
                luaType = lua_type(L, -1);
            }


            /* The current type must be the same as the last type */
            if ( luaType != lua_type(L, -1) )
            {
                lua_pop(L, 2);
                isArray = L2DBUS_FALSE;
                break;
            }

            /* We need to scrutinize some Lua types more than
             * others.
             */
            switch ( luaType )
            {
                case LUA_TNUMBER:
                case LUA_TBOOLEAN:
                case LUA_TSTRING:
                    break;

                case LUA_TTABLE:
                    /* A Lua table can either be mapped to a D-Bus dictionary,
                     * structure, or array. Since the criteria for a D-Bus dictionary
                     * is the loosest representation and if it can be represented as
                     * a D-Bus dictionary then a sufficient mapping exists. It
                     * would be redundant to verify that it can be mapped to
                     * other types.
                     */
                    if ( !l2dbus_dbusIsTableDictionary(L, -1)  )
                    {
                        isArray = L2DBUS_FALSE;
                    }
                    break;

                case LUA_TUSERDATA:
                    /* Get the string name of the userdata if it
                     * hasn't been found yet
                     */
                    if ( L2DBUS_INVALID_TYPE_ID == metaTypeId )
                    {
                        metaTypeId = l2dbus_getMetaTypeId(L, -1);

                        /* Make sure it's l2dbus userdata */
                        if ( NULL == l2dbus_isUserData(L, -1,
                            l2dbus_getNameByTypeId(metaTypeId)) )
                        {
                            isArray = L2DBUS_FALSE;
                        }

                        /* Now make sure it's a type of L2DBUS userdata
                         * that can be sent over D-Bus
                         */
                        if ( !(
                            (L2DBUS_INT64_TYPE_ID == metaTypeId) ||
                            (L2DBUS_UINT64_TYPE_ID == metaTypeId) ||
                            ((L2DBUS_START_DBUS_TYPE_ID < metaTypeId) &&
                             (L2DBUS_END_DBUS_TYPE_ID > metaTypeId) &&
                             (L2DBUS_DBUS_INVALID_TYPE_ID != metaTypeId))
                             ) )
                        {
                            isArray = L2DBUS_FALSE;
                        }
                    }

                    if ( isArray )
                    {
                        /* Now get the "current" userdata type name */
                        curMetaTypeId = l2dbus_getMetaTypeId(L, -1);
                        /* If the userdata types don't match then this
                         * isn't a valid homogeneous D-Bus array
                         */
                        if ( metaTypeId != curMetaTypeId )
                        {
                            isArray = L2DBUS_FALSE;
                        }
                    }
                    break;

                default:
                    isArray = L2DBUS_FALSE;
                    break;
            }

            if ( !isArray )
            {
                /* Pop the key/value pair */
                lua_pop(L, 2);
                break;
            }
            else
            {
                /* Get ready for the next go-around */
                lua_pop(L, 1);
            }
        }

        /* If this is an array then the number of elements that
         * Lua reports in the array should match what we've
         * counted.
         */
        if ( isArray && (itemCnt != lua_rawlen(L, tableIdx)) )
        {
            /* The number of items we counted
             * better equal the number the table reports.
             */
            isArray = L2DBUS_FALSE;
        }
    }

    return isArray;
}


/**
 * @brief Tests to see if a Lua table can be converted to a D-Bus structure.
 *
 * This function is used to determine whether a Lua table can be encoded as
 * a D-Bus structure. A D-Bus structure has the same requirements as a D-Bus
 * array with the exception that the types do not need to be of the same
 * homogeneous type.
 *
 * @param [in]      L       The Lua state.
 * @param [in]      idx     Index on the Lua stack where the Lua table is
 * located.
 *
 * @return Returns true if the table could be converted to a D-Bus structure,
 * otherwise false.
 */
static l2dbus_Bool
l2dbus_dbusIsTableStructure
    (
    lua_State*  L,
    int         idx
    )
{
    size_t itemCnt = 0;
    int tableIdx = lua_absindex(L, idx);
    l2dbus_Bool isStruct = L2DBUS_FALSE;
    int luaType = LUA_TNONE;
    l2dbus_TypeId metaTypeId = L2DBUS_INVALID_TYPE_ID;

    if ( LUA_TTABLE == lua_type(L, tableIdx) )
    {
        /* Now we'll assume this is a valid structure */
        isStruct = L2DBUS_TRUE;

        lua_pushnil(L);
        while ( lua_next(L, tableIdx) )
        {
            /* First make sure the key is a number*/
            if ( !lua_isnumber(L, -2) )
            {
                lua_pop(L, 2);
                isStruct = L2DBUS_FALSE;
                break;
            }
            /* Keep track of the number of items
             * in the array.
             */
            ++itemCnt;

            /* Now verify the value is a type that can
             * be marshalled over D-Bus
             */


            /* We need to scrutinize some Lua types more than
             * others.
             */
            luaType = lua_type(L, -1);
            switch ( luaType )
            {
                case LUA_TNUMBER:
                case LUA_TBOOLEAN:
                case LUA_TSTRING:
                    break;

                case LUA_TTABLE:
                    /* A Lua table can either be mapped to a D-Bus dictionary,
                     * structure, or array. Since the criteria for a D-Bus dictionary
                     * is the loosest representation and if it can be represented as
                     * a D-Bus dictionary then a sufficient mapping exists. It
                     * would be redundant too verify that it can be mapped to
                     * other types.
                     */
                    if ( !l2dbus_dbusIsTableDictionary(L, -1)  )
                    {
                        isStruct= L2DBUS_FALSE;
                    }
                    break;

                case LUA_TUSERDATA:
                    metaTypeId = l2dbus_getMetaTypeId(L, -1);

                    /* Make sure it's l2dbus userdata */
                    if ( NULL == l2dbus_isUserData(L, -1,
                        l2dbus_getNameByTypeId(metaTypeId)) )
                    {
                        isStruct = L2DBUS_FALSE;
                    }

                    /* Now make sure it's a type of L2DBUS userdata
                     * that can be sent over D-Bus
                     */
                    if ( !(
                        (L2DBUS_INT64_TYPE_ID == metaTypeId) ||
                        (L2DBUS_UINT64_TYPE_ID == metaTypeId) ||
                        ((L2DBUS_START_DBUS_TYPE_ID < metaTypeId) &&
                         (L2DBUS_END_DBUS_TYPE_ID > metaTypeId) &&
                         (L2DBUS_DBUS_INVALID_TYPE_ID != metaTypeId))
                         ) )
                    {
                        isStruct = L2DBUS_FALSE;
                    }
                    break;

                default:
                    isStruct = L2DBUS_FALSE;
                    break;
            }

            if ( !isStruct )
            {
                /* Pop the key/value pair */
                lua_pop(L, 2);
                break;
            }
            else
            {
                /* Get ready for the next go-around */
                lua_pop(L, 1);
            }
        }

        if ( isStruct && (itemCnt != lua_rawlen(L, tableIdx)) )
        {
            /* The number of items we counted
             * better equal the number the table reports.
             */
            isStruct = L2DBUS_FALSE;
        }
    }

    return isStruct;
}


/**
 * @brief Called by Lua VM to GC/reclaim the wrappted D-Bus userdata objects.
 *
 * This method is called by the Lua VM to reclaim the Lua userdata that
 * wraps D-Bus values.
 *
 * @return nil
 *
 */
static int
l2dbus_dbusTypeDispose
    (
    lua_State*  L
    )
{
    l2dbus_TypeId metaTypeId = l2dbus_getMetaTypeId(L, 1);
    const char* typeName = l2dbus_getNameByTypeId(metaTypeId);
    l2dbus_DbusValue* ud = (l2dbus_DbusValue*)luaL_checkudata(L, 1, typeName);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: %s (userdata=%p)", typeName, ud));
    lua_getuservalue(L, 1);
    luaL_unref(L, -1, ud->valueRef);
    ud->valueRef = LUA_NOREF;
    l2dbus_free(ud->signature);
    return 0;
}

/**
 The L2DBUS D-Bus *Invalid* class.
 @section Invalid
 */

/**
 The L2DBUS wrapper class for the D-Bus *Invalid* type.
 @type Invalid
 */

/**
 @function value
 @within Invalid

 Returns the value associated with the *Invalid* type.

 @treturn nil Returns **nil** for the *Invalid* type.
 */

/**
 @function dbusTypeId
 @within Invalid

 Returns the D-Bus type identifier associated with the *Invalid* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Invalid.new

 Creates a new D-Bus *Invalid* wrapper object.

 @treturn userdata A Lua userdata wrapper around a D-Bus *Invalid* typed object.
 */
static int
l2dbus_dbusNewInvalid
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    lua_pushnil(L);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_INVALID_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Byte* class.
 @section Byte
 */

/**
 The L2DBUS wrapper class for the D-Bus *Byte* type.
 @type Byte
 */

/**
 @function value
 @within Byte

 Returns the value associated with the *Byte* type.

 @treturn number Returns the value associated with the *Byte* type.
 */

/**
 @function dbusTypeId
 @within Byte

 Returns the D-Bus type identifier associated with the *Byte* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Byte.new

 Creates a new D-Bus *Byte* wrapper object.

 @tparam number value The Lua value associated and wrapped by the D-Bus
 *Byte* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Byte* typed object.
 */
static int
l2dbus_dbusNewByte
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_BYTE_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Boolean* type class.
 @section Boolean
 */

/**
 The L2DBUS wrapper class for the D-Bus *Boolean* type.
 @type Boolean
 */

/**
 @function value
 @within Boolean

 Returns the value associated with the *Boolean* type.

 @treturn bool Returns the value associated with the *Boolean* type.
 */

/**
 @function dbusTypeId
 @within Boolean

 Returns the D-Bus type identifier associated with the *Boolean* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Boolean.new

 Creates a new D-Bus *Boolean* wrapper object.

 @tparam bool value The Lua value associated and wrapped by the D-Bus
 *Boolean* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Boolean* typed object.
 */
static int
l2dbus_dbusNewBoolean
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_BOOLEAN_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Int16* class.
 @section Int16
 */

/**
 The L2DBUS wrapper class for the D-Bus *Int16* type.
 @type Int16
 */

/**
 @function value
 @within Int16

 Returns the value associated with the *Int16* type.

 @treturn number Returns the value associated with the *Int16* type.
 */

/**
 @function dbusTypeId
 @within Int16

 Returns the D-Bus type identifier associated with the *Int16* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Int16.new

 Creates a new D-Bus *Int16* wrapper object.

 @tparam number value The Lua value associated and wrapped by the D-Bus
 *Int16* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Int16* typed object.
 */
static int
l2dbus_dbusNewInt16
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_INT16_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Uint16* class.
 @section Uint16
 */

/**
 The L2DBUS wrapper class for the D-Bus *Uint16* type.
 @type Uint16
 */

/**
 @function value
 @within Uint16

 Returns the value associated with the *Uint16* type.

 @treturn number Returns the value associated with the *Uint16* type.
 */

/**
 @function dbusTypeId
 @within Uint16

 Returns the D-Bus type identifier associated with the *Uint16* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Uint16.new

 Creates a new D-Bus *Uint16* wrapper object.

 @tparam number value The Lua value associated and wrapped by the D-Bus
 *Uint16* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Uint16* typed object.
 */
static int
l2dbus_dbusNewUint16
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_UINT16_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Int32* class.
 @section Int32
 */

/**
 The L2DBUS wrapper class for the D-Bus *Int32* type.
 @type Int32
 */

/**
 @function value
 @within Int32

 Returns the value associated with the *Int32* type.

 @treturn number Returns the value associated with the *Int32* type.
 */

/**
 @function dbusTypeId
 @within Int32

 Returns the D-Bus type identifier associated with the *Int32* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Int32.new

 Creates a new D-Bus *Int32* wrapper object.

 @tparam number value The Lua value associated and wrapped by the D-Bus
 *Int32* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Int32* typed object.
 */
static int
l2dbus_dbusNewInt32
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_INT32_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Uint32* class.
 @section Uint32
 */

/**
 The L2DBUS wrapper class for the D-Bus *Uint32* type.
 @type Uint32
 */

/**
 @function value
 @within Uint32

 Returns the value associated with the *Uint32* type.

 @treturn number Returns the value associated with the *Uint32* type.
 */

/**
 @function dbusTypeId
 @within Uint32

 Returns the D-Bus type identifier associated with the *Uint32* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Uint32.new

 Creates a new D-Bus *Uint32* wrapper object.

 @tparam number value The Lua value associated and wrapped by the D-Bus
 *Uint32* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Uint32* typed object.
 */
static int
l2dbus_dbusNewUint32
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_UINT32_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Int64* class.
 @section Int64
 */

/**
 The L2DBUS wrapper class for the D-Bus *Int64* type.
 @type Int64
 */

/**
 @function value
 @within Int64

 Returns the value associated with the *Int64* type.

 @treturn number Returns the value associated with the *Int64* type.
 */

/**
 @function dbusTypeId
 @within Int64

 Returns the D-Bus type identifier associated with the *Int64* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Int64.new

 Creates a new D-Bus *Int64* wrapper object.

 @tparam ?number|userdata value The Lua value associated and wrapped by
 the D-Bus *Int64* type. Besides the basic Lua *number* type the constructor
 also accepts the @{l2dbus.Int64|Int64}/@{l2dbus.Uint64|Uint64} extended Lua
 types provided by this module.

 @treturn userdata A Lua userdata wrapper around a D-Bus *Int64* typed object.
 */
static int
l2dbus_dbusNewInt64
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    /* Create a copy of the converted type */
    l2dbus_int64Create(L, 1, 10);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_INT64_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Uint64* class.
 @section Uint64
 */

/**
 The L2DBUS wrapper class for the D-Bus *Uint64* type.
 @type Uint64
 */

/**
 @function value
 @within Uint64

 Returns the value associated with the *Uint64* type.

 @treturn number Returns the value associated with the *Uint64* type.
 */

/**
 @function dbusTypeId
 @within Uint64

 Returns the D-Bus type identifier associated with the *Uint64* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Uint64.new

 Creates a new D-Bus *Uint64* wrapper object.

 @tparam ?number|userdata value The Lua value associated and wrapped by
 the D-Bus *Uint64* type. Besides the basic Lua *number* type the constructor
 also accepts the @{l2dbus.Int64|Int64}/@{l2dbus.Uint64|Uint64} extended Lua
 types provided by this module.

 @treturn userdata A Lua userdata wrapper around a D-Bus *Uint64* typed object.
 */
static int
l2dbus_dbusNewUint64
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    l2dbus_uint64Create(L, 1, 10);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_UINT64_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Double* class.
 @section Double
 */

/**
 The L2DBUS wrapper class for the D-Bus *Double* type.
 @type Double
 */

/**
 @function value
 @within Double

 Returns the value associated with the *Double* type.

 @treturn number Returns the value associated with the *Double* type.
 */

/**
 @function dbusTypeId
 @within Double

 Returns the D-Bus type identifier associated with the *Double* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Double.new

 Creates a new D-Bus *Double* wrapper object.

 @tparam number value The Lua value associated and wrapped by the D-Bus
 *Double* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Double* typed object.
 */
static int
l2dbus_dbusNewDouble
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_DOUBLE_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *String* class.
 @section String
 */

/**
 The L2DBUS wrapper class for the D-Bus *String* type.
 @type String
 */

/**
 @function value
 @within String

 Returns the value associated with the *String* type.

 @treturn string Returns the value associated with the *String* type.
 */

/**
 @function dbusTypeId
 @within String

 Returns the D-Bus type identifier associated with the *String* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.String.new

 Creates a new D-Bus *String* wrapper object.

 @tparam string value The Lua value associated and wrapped by the D-Bus
 *String* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *String* typed object.
 */
static int
l2dbus_dbusNewString
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TSTRING);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_STRING_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *ObjectPath* class.
 @section ObjectPath
 */

/**
 The L2DBUS wrapper class for the D-Bus *ObjectPath* type.
 @type ObjectPath
 */

/**
 @function value
 @within ObjectPath

 Returns the value associated with the *ObjectPath* type.

 @treturn string Returns the value associated with the *ObjectPath* type.
 */

/**
 @function dbusTypeId
 @within ObjectPath

 Returns the D-Bus type identifier associated with the *ObjectPath* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.ObjectPath.new

 Creates a new D-Bus *ObjectPath* wrapper object. This function does **not**
 validate the supplied D-Bus object path and assumes it meets the D-Bus
 specification requirements for such a path.

 @tparam string value The Lua value associated and wrapped by the D-Bus
 *ObjectPath* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *ObjectPath* typed
 object.
 */
static int
l2dbus_dbusNewObjectPath
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TSTRING);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_OBJECT_PATH_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Signature* class.
 @section Signature
 */

/**
 The L2DBUS wrapper class for the D-Bus *Signature* type.
 @type Signature
 */

/**
 @function value
 @within Signature

 Returns the value associated with the *Signature* type.

 @treturn string Returns the value associated with the *Signature* type.
 */

/**
 @function dbusTypeId
 @within Signature

 Returns the D-Bus type identifier associated with the *Signature* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Signature.new

 Creates a new D-Bus *Signature* wrapper object. This function does **not**
 validate the supplied D-Bus type signature and assumes it meets the D-Bus
 specification requirements for such a signature.

 @tparam string value The Lua value associated and wrapped by the D-Bus
 *Signature* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Signature* typed
 object.
 */
static int
l2dbus_dbusNewSignature
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TSTRING);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_SIGNATURE_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/**
 The L2DBUS D-Bus *Array* class.
 @section Array
 */

/**
 The L2DBUS wrapper class for the D-Bus *Array* type.
 @type Array
 */

/**
 @function value
 @within Array

 Returns the value associated with the *Array* type.

 @treturn table Returns the value associated with the *Array* type.
 */

/**
 @function dbusTypeId
 @within Array

 Returns the D-Bus type identifier associated with the *Array* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Array.new

 Creates a new D-Bus *Array* wrapper object. This constructor only minimally
 validates the provided Lua table to verify that it meets the basic
 restrictions of a D-Bus array. At a minimum, the elements of the table must
 be consecutively indexed and be of the same (homogeneous) type.

 @tparam table value The Lua table associated and wrapped by the D-Bus
 *Array* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Array* typed
 object.
 */
static int
l2dbus_dbusNewArray
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    const char* signature;
    luaL_argcheck(L, l2dbus_dbusIsTableArray(L, 1), 1,
                "cannot convert argument to D-Bus array");
    signature = luaL_optstring(L, 2, NULL);

    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_ARRAY_TYPE_ID, signature);
    return l2dbus_dbusAttachValue(L, ud, -1, 1);
}


/**
 The L2DBUS D-Bus *Dictionary* class.
 @section Dictionary
 */

/**
 The L2DBUS wrapper class for the D-Bus *Dictionary* type.
 @type Dictionary
 */

/**
 @function value
 @within Dictionary

 Returns the value associated with the *Dictionary* type.

 @treturn table Returns the value associated with the *Dictionary* type.
 */

/**
 @function dbusTypeId
 @within Dictionary

 Returns the D-Bus type identifier associated with the *Dictionary* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Dictionary.new

 Creates a new D-Bus *Dictionary* wrapper object. In D-Bus terms a dictionary
 is an array of DICT_ENTRY structures whereby the key is a *basic* D-Bus type
 and the associated value is any valid D-Bus type. The constructor only
 minimally validates the provided Lua table to verify that it meets the basic
 restrictions of a D-Bus DICT_ENTRY array.

 @tparam table value The Lua table associated and wrapped by the D-Bus
 *Dictionary* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Dictionary* typed
 object.
 */
static int
l2dbus_dbusNewDictionary
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    const char* signature;
    luaL_argcheck(L, l2dbus_dbusIsTableDictionary(L, 1), 1,
                "cannot convert argument to D-Bus dictionary");
    signature = luaL_optstring(L, 2, NULL);

    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_DICT_ENTRY_TYPE_ID, signature);
    return l2dbus_dbusAttachValue(L, ud, -1, 1);
}


/**
 The L2DBUS D-Bus *Structure* class.
 @section Structure
 */

/**
 The L2DBUS wrapper class for the D-Bus *Structure* type.
 @type Structure
 */

/**
 @function value
 @within Structure

 Returns the value associated with the *Structure* type.

 @treturn table Returns the value associated with the *Structure* type.
 */

/**
 @function dbusTypeId
 @within Structure

 Returns the D-Bus type identifier associated with the *Structure* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Structure.new

 Creates a new D-Bus *Structure* wrapper object. In Lua terms this is an
 array containing non-homogeneous types. This differs from a D-Bus array
 where all the elements of the array must be the same time. The *fields* of
 the structure are ordered by increasing index. The constructor does
 minimal checking to verify the Lua table passed into it meets these minimal
 requirements.

 @tparam table value The Lua table associated and wrapped by the D-Bus
 *Structure* type.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Structure* typed
 object.
 */
static int
l2dbus_dbusNewStructure
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    const char* signature;
    luaL_argcheck(L, l2dbus_dbusIsTableStructure(L, 1), 1,
                "cannot convert argument to D-Bus structure");
    signature = luaL_optstring(L, 2, NULL);

    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_STRUCT_TYPE_ID, signature);
    return l2dbus_dbusAttachValue(L, ud, -1, 1);
}


/**
 The L2DBUS D-Bus *Variant* class.
 @section Variant
 */

/**
 The L2DBUS wrapper class for the D-Bus *Variant* type.
 @type Variant
 */

/**
 @function value
 @within Variant

 Returns the value associated with the *Variant* type.

 @treturn table Returns the value associated with the *Variant* type.
 */

/**
 @function dbusTypeId
 @within Variant

 Returns the D-Bus type identifier associated with the *Variant* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.Variant.new

 Creates a new D-Bus *Variant* wrapper object. A variant can hold any D-Bus
 type whether it is a basic or container type. This means it can be used
 to hold any Lua type **excluding** functions, threads (coroutines), nil, and
 external userdata (or lightuserdata) objects.

 @tparam any value The Lua value (any type) associated and wrapped by the D-Bus
 *Variant* type. This type **excludes** functions, threads, nil, and external
 userdata or lightuserdata.
 @tparam ?string signature The optional D-Bus signature describing the
 variant type. **Note:** it's necessary to prefix the signature with the
 D-Bus '*v*' (variant) type code in order to be parsed as a variant.
 @treturn userdata A Lua userdata wrapper around a D-Bus *Variant* typed
 object.
 */
static int
l2dbus_dbusNewVariant
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    const char* signature;
    l2dbus_Bool isValid = L2DBUS_TRUE;
    int luaType = lua_type(L, 1);
    l2dbus_TypeId metaTypeId;

    switch ( luaType )
    {
        case LUA_TNUMBER:
        case LUA_TBOOLEAN:
        case LUA_TSTRING:
            break;

        case LUA_TTABLE:
            /* A Lua table can either be mapped to a D-Bus dictionary,
             * structure, or array. Since the criteria for a D-Bus dictionary
             * is the loosest representation and if it can be represented as
             * a D-Bus dictionary then a sufficient mapping exists. It
             * would be redundant too verify that it can be mapped to
             * other types.
             */
            if ( !l2dbus_dbusIsTableDictionary(L, 1)  )
            {
                isValid= L2DBUS_FALSE;
            }
            break;

        case LUA_TUSERDATA:
            metaTypeId = l2dbus_getMetaTypeId(L, 1);

            /* Make sure it's l2dbus userdata */
            if ( NULL == l2dbus_isUserData(L, 1,
                l2dbus_getNameByTypeId(metaTypeId)) )
            {
                isValid = L2DBUS_FALSE;
            }

            /* Now make sure it's a type of L2DBUS userdata
             * that can be sent over D-Bus
             */
            if ( !(
                (L2DBUS_INT64_TYPE_ID == metaTypeId) ||
                (L2DBUS_UINT64_TYPE_ID == metaTypeId) ||
                ((L2DBUS_START_DBUS_TYPE_ID < metaTypeId) &&
                 (L2DBUS_END_DBUS_TYPE_ID > metaTypeId) &&
                 (L2DBUS_DBUS_INVALID_TYPE_ID != metaTypeId))
                 ) )
            {
                isValid = L2DBUS_FALSE;
            }
            break;

        default:
            isValid = L2DBUS_FALSE;
            break;
    }

    luaL_argcheck(L, isValid, 1, "cannot convert to D-Bus type");
    signature = luaL_optstring(L, 2, NULL);

    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_VARIANT_TYPE_ID, signature);
    return l2dbus_dbusAttachValue(L, ud, -1, 1);
}


/**
 The L2DBUS D-Bus *UnixFd* class.
 @section UnixFd
 */

/**
 The L2DBUS wrapper class for the D-Bus *UnixFd* type.
 @type UnixFd
 */

/**
 @function value
 @within UnixFd

 Returns the value associated with the *UnixFd* type.

 @treturn number Returns the value associated with the *UnixFd* type.
 */

/**
 @function dbusTypeId
 @within UnixFd

 Returns the D-Bus type identifier associated with the *UnixFd* type.

 @treturn number Returns the integral value for the associated D-Bus type.
 */

/**
 * @section end
 */

/**
 @function l2dbus.DbusTypes.UnixFd.new

 Creates a new D-Bus *UnixFd* wrapper object.

 @tparam number fd A Unix file descriptor to wrap. The application must
 verify that the version of D-Bus being used supports passing file descriptors
 since not all versions of the library support it.

 @treturn userdata A Lua userdata wrapper around a D-Bus *UnixFd* typed object.
 */
static int
l2dbus_dbusNewUnixFd
    (
    lua_State*  L
    )
{
    l2dbus_DbusValue* ud;
    luaL_checktype(L, 1, LUA_TNUMBER);
    ud = l2dbus_dbusNewUserdata(L, L2DBUS_DBUS_UNIX_FD_TYPE_ID, NULL);
    return l2dbus_dbusAttachValue(L, ud, -1, -2);
}


/*
 * Local structure used to create D-Bus type wrapper objects.
 */
typedef struct l2dbus_DbusTypeItem
{
    const char*         name;
    l2dbus_TypeId       metaTypeId;
    const luaL_Reg*     metaFuncs;
    int                 dbusTypeId;
    lua_CFunction       ctor;
} l2dbus_DbusTypeItem;

/*
 * The methods of the l2dbus.DbusTypes.XXXX type.
 */
const luaL_Reg l2dbus_dbusTypeBasicMeta[] = {
    {"value", l2dbus_dbusGetLuaValue},
    {"dbusTypeId", l2dbus_dbusGetTypeId},
    {"__tostring", l2dbus_dbusToString},
    {"__gc", l2dbus_dbusTypeDispose},
    {NULL, NULL}
};


static const l2dbus_DbusTypeItem gDbusTypeRegistry[] = {
    {"Invalid", L2DBUS_DBUS_INVALID_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_INVALID, l2dbus_dbusNewInvalid},
    {"Byte", L2DBUS_DBUS_BYTE_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_BYTE, l2dbus_dbusNewByte},
    {"Boolean", L2DBUS_DBUS_BOOLEAN_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_BOOLEAN, l2dbus_dbusNewBoolean},
    {"Int16", L2DBUS_DBUS_INT16_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_INT16, l2dbus_dbusNewInt16},
    {"Uint16", L2DBUS_DBUS_UINT16_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_UINT16, l2dbus_dbusNewUint16},
    {"Int32", L2DBUS_DBUS_INT32_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_INT32, l2dbus_dbusNewInt32},
    {"Uint32", L2DBUS_DBUS_UINT32_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_UINT32, l2dbus_dbusNewUint32},
    {"Int64", L2DBUS_DBUS_INT64_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_INT64, l2dbus_dbusNewInt64},
    {"Uint64", L2DBUS_DBUS_UINT64_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_UINT64, l2dbus_dbusNewUint64},
    {"Double", L2DBUS_DBUS_DOUBLE_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_DOUBLE, l2dbus_dbusNewDouble},
    {"String", L2DBUS_DBUS_STRING_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_STRING, l2dbus_dbusNewString},
    {"ObjectPath", L2DBUS_DBUS_OBJECT_PATH_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_OBJECT_PATH, l2dbus_dbusNewObjectPath},
    {"Signature", L2DBUS_DBUS_SIGNATURE_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_SIGNATURE, l2dbus_dbusNewSignature},
    {"Array", L2DBUS_DBUS_ARRAY_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_ARRAY, l2dbus_dbusNewArray},
    {"Structure", L2DBUS_DBUS_STRUCT_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_STRUCT, l2dbus_dbusNewStructure},
    {"Variant", L2DBUS_DBUS_VARIANT_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_VARIANT, l2dbus_dbusNewVariant},
    {"Dictionary", L2DBUS_DBUS_DICT_ENTRY_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_DICT_ENTRY, l2dbus_dbusNewDictionary},
    {"UnixFd", L2DBUS_DBUS_UNIX_FD_TYPE_ID, l2dbus_dbusTypeBasicMeta, DBUS_TYPE_UNIX_FD, l2dbus_dbusNewUnixFd},
};


/**
 * @brief Calculates the best D-Bus type to represent a Lua number.
 *
 * The "best" D-Bus type to represent a Lua number is one that (ideally) does
 * not truncate it's value while providing an efficient and predictable
 * mapping between the two representations.
 *
 * @param [in] value    The Lua number to represent as a D-Bus type.
 * @return The D-Bus type best suited to represent a Lua number.
 */
static int
l2dbus_calcDbusNumType
    (
    lua_Number value
    )
{
    int dbusType = DBUS_TYPE_INVALID;
    lua_Number whole;
    lua_Number frac;

#ifdef LUA_NUMBER_DOUBLE
    frac = modf(value, &whole);
#elif defined LUA_NUMBER_FLOAT
    frac = modff(value, &whole);
#else
    /* Assume it's an integral type */
    frac = 0;
    whole = value;
#endif

    /* If there is a fractional part to it then */
    if ( frac != 0 )
    {
        dbusType = DBUS_TYPE_DOUBLE;
    }
    /* Else no fractional parts - whole number */
    else
    {
        /* If this is a positive number then ... */
        if ( whole >= 0 )
        {
            if ( whole <= INT32_MAX )
            {
                dbusType = DBUS_TYPE_INT32;
            }
            else if ( whole <= UINT32_MAX )
            {
                dbusType = DBUS_TYPE_UINT32;
            }
            else if ( whole <= L2DBUS_MAX_INTEGRAL_LUA_NUM )
            {
                dbusType = DBUS_TYPE_INT64;
            }
            else
            {
                dbusType = DBUS_TYPE_DOUBLE;
            }
        }
        /* Else a negative number */
        else
        {
            if ( whole >= INT32_MIN )
            {
                dbusType = DBUS_TYPE_INT32;
            }
            else if ( whole >= L2DBUS_MIN_INTEGRAL_LUA_NUM )
            {
                dbusType = DBUS_TYPE_INT64;
            }
            else
            {
                dbusType = DBUS_TYPE_DOUBLE;
            }
        }
    }

    return dbusType;
}


/**
 * @brief Maps a Lua type to an equivalent D-Bus type.
 *
 * Tries to map a Lua type to an equivalent D-Bus type. If an error
 * is detected then a Lua error will be generated.
 *
 * @param [in] L    The Lua number to represent as a D-Bus type.
 * @param [in] idx  The index of the Lua value on the Lua stack.
 * @return The D-Bus type best suited to represent the Lua type.
 */
static int
l2dbus_transcodeMapLuaToDbusType
    (
    lua_State*  L,
    int         idx
    )
{
    int dbusType = DBUS_TYPE_INVALID;
    l2dbus_TypeId metaTypeId;

    switch ( lua_type(L, idx) )
    {
        case LUA_TNUMBER:
            dbusType = l2dbus_calcDbusNumType(lua_tonumber(L, idx));
            break;

        case LUA_TBOOLEAN:
            dbusType = DBUS_TYPE_BOOLEAN;
            break;

        case LUA_TSTRING:
            dbusType = DBUS_TYPE_STRING;
            break;

        case LUA_TTABLE:
            /* Check from the most restrictive to the least */
            if ( l2dbus_dbusIsTableArray(L, idx) )
            {
                dbusType = DBUS_TYPE_ARRAY;
            }
            else if ( l2dbus_dbusIsTableStructure(L, idx) )
            {
                dbusType = DBUS_TYPE_STRUCT;
            }
            else if ( l2dbus_dbusIsTableDictionary(L, idx) )
            {
                dbusType = DBUS_TYPE_DICT_ENTRY;
            }
            break;

        case LUA_TUSERDATA:
            metaTypeId = l2dbus_getMetaTypeId(L, idx);
            if ( L2DBUS_INT64_TYPE_ID == metaTypeId )
            {
                dbusType = DBUS_TYPE_INT64;
            }
            else if ( L2DBUS_UINT64_TYPE_ID == metaTypeId )
            {
                dbusType = DBUS_TYPE_UINT64;
            }
            else if ( !l2dbus_dbusQueryDbusTypeId(L, idx, &dbusType) )
            {
                dbusType = DBUS_TYPE_INVALID;
            }
            break;

        default:
            break;
    }

    return dbusType;
}


/**
 * @brief A "shim" function that allows a Lua protected call to be made.
 *
 * This shim function wraps the l2dbus_transcodeMarshallAsType() function so
 * that it can recursively called via a Lua pcall. Any Lua errors can be
 * "caught" before being propagated to the user application. On entry to the
 * function the Lua stack must hold the argument to marshall as well as
 * lightuserdata for the message and signature iterators.
 *
 * @param [in] L    The Lua number to represent as a D-Bus type.
 */
static int
l2dbus_transcodeMarshallAsTypeShim
    (
    lua_State*  L
    )
{
    DBusMessageIter*    msgIt;
    DBusSignatureIter*  sigIt;

    msgIt = lua_touserdata(L, -2);
    sigIt = lua_touserdata(L, -1);
    lua_pop(L, 2);
    l2dbus_transcodeMarshallAsType(L, 1, msgIt, sigIt);

    return 0;
}


/**
 * @brief Marshalls a Lua argument into a D-Bus message.
 *
 * This function takes an arbitrary Lua argument and encodes/marshalls it
 * into a D-Bus message using the D-Bus signature as a guide for converting
 * the types correctly. Any errors encountered while encoding the argument
 * will result in a Lua error being thrown.
 *
 * @param [in] L        The Lua state.
 * @param [in] argIdx   The Lua stack index of the argument.
 * @param [in] msgIt    Pointer to a D-Bus message iterator.
 * @param [in] sigIt    Pointer to a D-Bus signature iterator.
 */
static void
l2dbus_transcodeMarshallAsType
    (
    lua_State*          L,
    int                 argIdx,
    DBusMessageIter*    msgIt,
    DBusSignatureIter*  sigIt
    )
{
    int origTop = lua_gettop(L);
    uint8_t uint8Value;
    int16_t int16Value;
    uint16_t uint16Value;
    int32_t int32Value;
    uint32_t uint32Value;
    int64_t int64Value;
    uint64_t uint64Value;
    double doubleValue;
    const char* strValue;
    l2dbus_Int64* int64Ud;
    l2dbus_Uint64* uint64Ud;
    DBusSignatureIter sigSubIt;
    DBusMessageIter msgSubIt;
    cdbus_StringBuffer* sigBuf = NULL;
    char* signature;
    const char* cachedSig = NULL;
    size_t  arrayLen;
    size_t  idx;
    argIdx = lua_absindex(L, argIdx);
    int dbusType = dbus_signature_iter_get_current_type(sigIt);

    /* If this is a D-Bus wrapper class then ... */
    if ( LUA_TUSERDATA == lua_type(L, argIdx) )
    {
        cachedSig = l2dbus_dbusGetCachedSignature(L, argIdx);
        l2dbus_transcodeGetValue(L, argIdx);
        argIdx = lua_absindex(L, -1);
    }

    switch ( dbusType )
    {
        case DBUS_TYPE_BYTE:
            if ( lua_isnumber(L, argIdx) )
            {
                uint8Value = (uint8_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                uint8Value = (uint8_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                uint8Value = (uint8_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_BYTE",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &uint8Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_BYTE");
            }
            break;

        case DBUS_TYPE_BOOLEAN:
            if ( lua_isboolean(L, argIdx) )
            {
                int32Value = (int32_t)lua_toboolean(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                int32Value = (int32_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                int32Value = (int32_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_BOOLEAN",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &int32Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_BOOLEAN");
            }
            break;

        case DBUS_TYPE_INT16:
            if ( lua_isnumber(L, argIdx) )
            {
                int16Value = (int16_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                int16Value = (int16_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                int16Value = (int16_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_INT16",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &int16Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_INT16");
            }
            break;

        case DBUS_TYPE_UINT16:
            if ( lua_isnumber(L, argIdx) )
            {
                uint16Value = (uint16_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                uint16Value = (uint16_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                uint16Value = (uint16_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_UINT16",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &uint16Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_UINT16");
            }
            break;

        case DBUS_TYPE_INT32:
            if ( lua_isnumber(L, argIdx) )
            {
                int32Value = (int32_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                int32Value = (int32_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                int32Value = (int32_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_INT32",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &int32Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_INT32");
            }
            break;

        case DBUS_TYPE_UINT32:
            if ( lua_isnumber(L, argIdx) )
            {
                uint32Value = (uint32_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                uint32Value = (uint32_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                uint32Value = (uint32_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_UINT32",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType,
                &uint32Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_UINT32");
            }
            break;

        case DBUS_TYPE_INT64:
            if ( lua_isnumber(L, argIdx) )
            {
                int64Value = (int64_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_INT64_MTBL_NAME)) != NULL )
            {
                int64Value = int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                int64Value = (int64_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_INT64",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &int64Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_INT64");
            }
            break;

        case DBUS_TYPE_UINT64:
            if ( lua_isnumber(L, argIdx) )
            {
                uint64Value = (uint64_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_INT64_MTBL_NAME)) != NULL )
            {
                uint64Value = (uint64_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                uint64Value = uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_UINT64",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType,
                &uint64Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_UINT64");
            }
            break;

        case DBUS_TYPE_DOUBLE:
            if ( lua_isnumber(L, argIdx) )
            {
                doubleValue = (double)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                doubleValue = (double)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                doubleValue = (double)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_DOUBLE",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType,
                &doubleValue) )
            {
                luaL_error(L, "could not append DBUS_TYPE_DOUBLE");
            }
            break;

        case DBUS_TYPE_STRING:
            strValue = luaL_checkstring(L, argIdx);
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &strValue) )
            {
                luaL_error(L, "could not append DBUS_TYPE_STRING");
            }
            break;

        case DBUS_TYPE_OBJECT_PATH:
            strValue = luaL_checkstring(L, argIdx);
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &strValue) )
            {
                luaL_error(L, "could not append DBUS_TYPE_OBJECT_PATH");
            }
            break;

        case DBUS_TYPE_SIGNATURE:
            strValue = luaL_checkstring(L, argIdx);
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &strValue) )
            {
                luaL_error(L, "could not append DBUS_TYPE_SIGNATURE");
            }
            break;

        case DBUS_TYPE_ARRAY:
            luaL_checktype(L, argIdx, LUA_TTABLE);
            dbus_signature_iter_recurse(sigIt, &sigSubIt);
            signature = dbus_signature_iter_get_signature(&sigSubIt);
            if ( !dbus_message_iter_open_container(msgIt, dbusType, signature,
                &msgSubIt) )
            {
                dbus_free(signature);
                luaL_error(L, "could not open D-Bus container for array");
            }
            dbus_free(signature);

            if ( DBUS_TYPE_DICT_ENTRY ==
                dbus_signature_iter_get_current_type(&sigSubIt) )
            {
                l2dbus_transcodeMarshallAsType(L, argIdx, &msgSubIt, &sigSubIt);
            }
            else
            {
                arrayLen = lua_rawlen(L, argIdx);
                for ( idx = 1;
                    (idx <= arrayLen) &&
                    (DBUS_TYPE_INVALID !=
                        dbus_signature_iter_get_current_type(&sigSubIt));
                    ++idx )
                {
                    lua_rawgeti(L, argIdx, idx);
                    l2dbus_transcodeMarshallAsType(L, -1, &msgSubIt, &sigSubIt);
                    /* It's not necessary to iterate to the next type in
                     * the signature because in D-Bus *all* the values of
                     * an array must be the same time (e.g. homogeneous
                     * typed arrays).
                     */
                    lua_pop(L, 1);
                }
            }

            if ( !dbus_message_iter_close_container(msgIt, &msgSubIt) )
            {
                luaL_error(L, "could not close D-Bus container for array");
            }
            break;

        case DBUS_TYPE_STRUCT:
            luaL_checktype(L, argIdx, LUA_TTABLE);
            dbus_signature_iter_recurse(sigIt, &sigSubIt);
            if ( !dbus_message_iter_open_container(msgIt, dbusType, NULL,
                &msgSubIt) )
            {
                luaL_error(L, "could not open D-Bus container for structure");
            }

            arrayLen = lua_rawlen(L, argIdx);
            for ( idx = 1;
                (idx <= arrayLen) &&
                (DBUS_TYPE_INVALID !=
                    dbus_signature_iter_get_current_type(&sigSubIt));
                ++idx )
            {
                lua_rawgeti(L, argIdx, idx);
                l2dbus_transcodeMarshallAsType(L, -1, &msgSubIt, &sigSubIt);
                dbus_signature_iter_next(&sigSubIt);
                lua_pop(L, 1);
            }

            if ( !dbus_message_iter_close_container(msgIt, &msgSubIt) )
            {
                luaL_error(L, "could not close D-Bus container for structure");
            }
            break;

        case DBUS_TYPE_VARIANT:
            if ( NULL != cachedSig )
            {
                /* For a variant we're really only interested in
                 * the signature *after* the 'v'. Let's strip that
                 * away.
                 */
                if ( DBUS_TYPE_VARIANT == cachedSig[0] )
                {
                    cachedSig = cachedSig + 1;
                }
                sigBuf = cdbus_stringBufferCopy(cachedSig);
            }
            else
            {
                sigBuf = cdbus_stringBufferNew(L2DBUS_DEFAULT_SIGNATURE_LENGTH);
            }

            if ( NULL == sigBuf )
            {
                luaL_error(L, "failed to allocate memory for signature buffer");
            }

            /* If a cached copy of the signature doesn't exist then */
            if ( NULL == cachedSig )
            {
                if ( !l2dbus_dbusComputeSignature(L, argIdx, sigBuf, 0) ||
                    !dbus_signature_validate(cdbus_stringBufferRaw(sigBuf),
                    NULL) )
                {
                    cdbus_stringBufferUnref(sigBuf);
                    luaL_error(L,
                        "failed to compute signature of variant type");
                }
            }

            if ( !dbus_message_iter_open_container(msgIt, dbusType,
                cdbus_stringBufferRaw(sigBuf), &msgSubIt) )
            {
                cdbus_stringBufferUnref(sigBuf);
                luaL_error(L, "could not open D-Bus container for variant");
            }

            dbus_signature_iter_init(&sigSubIt, cdbus_stringBufferRaw(sigBuf));

            /* We have to make a protected recursive call here because
             * the signature buffer is on the heap and if an error
             * occurs we need a chance to free up the signature before
             * propagating the error on up to Lua.
             */
            lua_pushcfunction(L, l2dbus_transcodeMarshallAsTypeShim);
            lua_pushvalue(L, argIdx);
            lua_pushlightuserdata(L, &msgSubIt);
            lua_pushlightuserdata(L, &sigSubIt);
            if ( 0 != lua_pcall(L, 3, 0, 0) )
            {
                /* Free the signature buffer before the error long jump */
                cdbus_stringBufferUnref(sigBuf);

                /* Propagate the Lua error */
                lua_error(L);
            }

            /* Free the buffer in case a Lua error occurs */
            cdbus_stringBufferUnref(sigBuf);

            if ( !dbus_message_iter_close_container(msgIt, &msgSubIt) )
            {
                luaL_error(L, "could not close D-Bus container for variant");
            }
            break;

        case DBUS_TYPE_DICT_ENTRY:
            luaL_checktype(L, argIdx, LUA_TTABLE);

            lua_pushnil(L);
            while ( lua_next(L, argIdx) )
            {
                dbus_signature_iter_recurse(sigIt, &sigSubIt);
                if ( !dbus_message_iter_open_container(msgIt, dbusType, NULL,
                    &msgSubIt) )
                {
                    luaL_error(L,
                        "could not open D-Bus container for dictionary");
                }

                /* Encode the key */
                l2dbus_transcodeMarshallAsType(L, -2, &msgSubIt, &sigSubIt);
                if ( !dbus_signature_iter_next(&sigSubIt) )
                {
                    luaL_error(L, "invalid dictionary signature");
                }
                else
                {
                    /* Encode the value */
                    l2dbus_transcodeMarshallAsType(L, -1, &msgSubIt, &sigSubIt);
                }

                if ( !dbus_message_iter_close_container(msgIt, &msgSubIt) )
                {
                    luaL_error(L,
                        "could not close D-Bus container for dictionary");
                }

                /* Pop off the value for the next go-around */
                lua_pop(L, 1);
            }
            break;

        case DBUS_TYPE_UNIX_FD:
            if ( lua_isnumber(L, argIdx) )
            {
                int32Value = (int32_t)lua_tonumber(L, argIdx);
            }
            else if ( (int64Ud = l2dbus_isUserData(L, argIdx,
                    L2DBUS_DBUS_INT64_MTBL_NAME)) != NULL )
            {
                int32Value = (int32_t)int64Ud->value;
            }
            else if ( (uint64Ud = l2dbus_isUserData(L, argIdx,
                L2DBUS_DBUS_UINT64_MTBL_NAME)) != NULL )
            {
                int32Value = (int32_t)uint64Ud->value;
            }
            else
            {
                luaL_error(L, "cannot convert %s to a DBUS_TYPE_UNIX_FD",
                    lua_typename(L, lua_type(L, argIdx)));
            }
            if ( !dbus_message_iter_append_basic(msgIt, dbusType, &int32Value) )
            {
                luaL_error(L, "could not append DBUS_TYPE_UNIX_FD");
            }
            break;

        default:
            luaL_error(L, "cannot convert signature");
            break;
    }

    lua_settop(L, origTop);
}


/**
 * @brief Unmarshalls the D-Bus message parameters into a Lua argument array.
 *
 * This function takes a D-Bus message and decodes/unmarshalls the parameters
 * into their corresponding Lua argument types that are appended to an array.
 * If an error is encountered unmarshalling the parameters then a Lua error
 * is thrown.
 *
 * @param [in] L        The Lua state.
 * @param [in] iter     Pointer to a D-Bus message iterator.
 * @param [in] tableIdx The Lua stack index of the returned Lua table/array.
 * @param [in] arrIdx   The current index into the Lua table/array.
 */
static void
l2dbus_transcodeUnmarshall
    (
    lua_State*          L,
    DBusMessageIter*    iter,
    int                 tableIdx,
    int*                arrIdx
    )
{
    uint8_t uint8Value;
    int16_t int16Value;
    uint16_t uint16Value;
    int32_t int32Value;
    uint32_t uint32Value;
    l2dbus_Int64* int64Ud;
    l2dbus_Uint64* uint64Ud;
    double doubleValue;
    const char* strValue;
    DBusMessageIter subIter;
    int subIdx;
    l2dbus_Bool skipArrayAdd = L2DBUS_FALSE;
    tableIdx = lua_absindex(L, tableIdx);
    int dbusType = dbus_message_iter_get_arg_type(iter);

    if ( DBUS_TYPE_INVALID != dbusType )
    {
        switch ( dbusType )
        {
            case DBUS_TYPE_BYTE:
                dbus_message_iter_get_basic(iter, &uint8Value);
                lua_pushnumber(L, uint8Value);
                break;

            case DBUS_TYPE_BOOLEAN:
                dbus_message_iter_get_basic(iter, &int32Value);
                lua_pushboolean(L, int32Value);
                break;

            case DBUS_TYPE_INT16:
                dbus_message_iter_get_basic(iter, &int16Value);
                lua_pushnumber(L, int16Value);
                break;

            case DBUS_TYPE_UINT16:
                dbus_message_iter_get_basic(iter, &uint16Value);
                lua_pushnumber(L, uint16Value);
                break;

            case DBUS_TYPE_INT32:
                dbus_message_iter_get_basic(iter, &int32Value);
                lua_pushnumber(L, int32Value);
                break;

            case DBUS_TYPE_UINT32:
                dbus_message_iter_get_basic(iter, &uint32Value);
                lua_pushnumber(L, uint32Value);
                break;

            case DBUS_TYPE_INT64:
                int64Ud = l2dbus_objectNew(L, sizeof(*int64Ud),
                                            L2DBUS_INT64_TYPE_ID);
                dbus_message_iter_get_basic(iter, &int64Ud->value);
                break;

            case DBUS_TYPE_UINT64:
                uint64Ud = l2dbus_objectNew(L, sizeof(*uint64Ud),
                                            L2DBUS_UINT64_TYPE_ID);
                dbus_message_iter_get_basic(iter, &uint64Ud->value);
                break;

            case DBUS_TYPE_DOUBLE:
                dbus_message_iter_get_basic(iter, &doubleValue);
                lua_pushnumber(L, doubleValue);
                break;

            case DBUS_TYPE_STRING:
                dbus_message_iter_get_basic(iter, &strValue);
                lua_pushstring(L, strValue);
                break;

            case DBUS_TYPE_OBJECT_PATH:
                dbus_message_iter_get_basic(iter, &strValue);
                lua_pushstring(L, strValue);
                break;

            case DBUS_TYPE_SIGNATURE:
                dbus_message_iter_get_basic(iter, &strValue);
                lua_pushstring(L, strValue);
                break;

            case DBUS_TYPE_ARRAY:
                lua_newtable(L);
                subIdx = 1;
                dbus_message_iter_recurse(iter, &subIter);
                while ( DBUS_TYPE_INVALID !=
                    dbus_message_iter_get_arg_type(&subIter) )
                {
                    l2dbus_transcodeUnmarshall(L, &subIter, -1, &subIdx);
                    dbus_message_iter_next(&subIter);
                }
                break;

            case DBUS_TYPE_STRUCT:
                lua_newtable(L);
                subIdx = 1;
                dbus_message_iter_recurse(iter, &subIter);
                while ( DBUS_TYPE_INVALID !=
                    dbus_message_iter_get_arg_type(&subIter) )
                {
                    l2dbus_transcodeUnmarshall(L, &subIter, -1, &subIdx);
                    dbus_message_iter_next(&subIter);
                }
                break;

            case DBUS_TYPE_VARIANT:
                dbus_message_iter_recurse(iter, &subIter);
                l2dbus_transcodeUnmarshall(L, &subIter, tableIdx, arrIdx);
                skipArrayAdd = L2DBUS_TRUE;
                break;

            case DBUS_TYPE_DICT_ENTRY:
                dbus_message_iter_recurse(iter, &subIter);
                /* Create a temporary array to hold the key at
                 * index 1 and the value at index 2
                 */
                lua_createtable(L, 2, 0);
                subIdx = 1;
                /* Demarshall they key */
                l2dbus_transcodeUnmarshall(L, &subIter, -1, &subIdx);
                if ( !dbus_message_iter_next(&subIter) )
                {
                    luaL_error(L,
                        "missing value in D-Bus dictionary signature");
                }
                /* Demarshall the value */
                l2dbus_transcodeUnmarshall(L, &subIter, -1, &subIdx);
                /* Push the key which is store in index 1 */
                lua_rawgeti(L, -1, 1);
                /* Push the value stored at index 2*/
                lua_rawgeti(L, -2, 2);
                lua_settable(L, tableIdx);
                /* Remove the temporary table */
                lua_pop(L, 1);
                skipArrayAdd = L2DBUS_TRUE;
                break;

            case DBUS_TYPE_UNIX_FD:
                dbus_message_iter_get_basic(iter, &int32Value);
                lua_pushnumber(L, int32Value);
                break;

            default:
                L2DBUS_TRACE((L2DBUS_TRC_WARN,
                        "Unsupported D-Bus type to unmarshall (%d)", dbusType));
                skipArrayAdd = L2DBUS_TRUE;
                break;
        }

        if ( !skipArrayAdd )
        {
            lua_rawseti(L, tableIdx, *arrIdx);
            *arrIdx += 1;
        }
    }
}


/**
 * @brief Appends Lua arguments to a D-Bus message using a signature.
 *
 * This function takes arguments on the Lua stack and marshalls them into
 * a D-Bus message using a D-Bus type signature as a guide on how the types
 * should be converted from Lua to D-Bus. If an error is encountered then
 * this function will throw an Lua error.
 *
 * @param [in] L            The Lua state.
 * @param [in] msg          The message the arguments will be appended to.
 * @param [in] argIdx       The Lua stack index where the arguments start.
 * @param [in] nArgs        The number of arguments on the Lua stack to
 * marshall.
 * @param [in] signature    The D-Bus message signature used to encode the
 * arguments.
 */
void
l2dbus_transcodeLuaArgsToDbusBySignature
    (
    lua_State*      L,
    DBusMessage*    msg,
    int             argIdx,
    int             nArgs,
    const char*     signature
    )
{
    DBusMessageIter msgIt;
    DBusSignatureIter sigIt;
    int argLast;
    argIdx = lua_absindex(L, argIdx);
    argLast = argIdx + nArgs;

    if ( NULL == msg )
    {
        luaL_error(L, "no D-Bus message provided");
    }

    if ( NULL == signature )
    {
        luaL_error(L, "no signature provided");
    }

    if ( !dbus_signature_validate(signature, NULL) )
    {
        luaL_error(L, "invalid D-Bus message signature (*s)", signature);
    }

    if ( nArgs > 0 )
    {
        dbus_message_iter_init_append(msg, &msgIt);
        dbus_signature_iter_init(&sigIt, signature);

        do
        {
            l2dbus_transcodeMarshallAsType(L, argIdx, &msgIt, &sigIt);
            ++argIdx;
        }
        while ( dbus_signature_iter_next(&sigIt) && (argIdx < argLast) );

        if ( argIdx != argLast )
        {
            luaL_error(L, "argument/signature mismatch");
        }
    }
}



/**
 * @brief Appends Lua arguments to a D-Bus message without a signature.
 *
 * This function takes arguments on the Lua stack and marshalls them into
 * a D-Bus message without a provide signature. The signature is computed
 * using a heuristic technique to map Lua types to suitable D-Bus types. If
 * an error is encountered then this function will throw an Lua error.
 *
 * @param [in] L            The Lua state.
 * @param [in] msg          The message the arguments will be appended to.
 * @param [in] argIdx       The Lua stack index where the arguments start.
 * @param [in] nArgs        The number of arguments on the Lua stack to
 * marshall.
 */
void
l2dbus_transcodeLuaArgsToDbus
    (
    lua_State*      L,
    DBusMessage*    msg,
    int             argIdx,
    int             nArgs
    )
{
    DBusMessageIter msgIt;
    DBusSignatureIter sigIt;
    int idx;
    cdbus_StringBuffer* sigBuf = NULL;

    if ( NULL == msg )
    {
        luaL_error(L, "no D-Bus message provided");
    }

    sigBuf = cdbus_stringBufferNew(L2DBUS_DEFAULT_SIGNATURE_LENGTH);
    dbus_message_iter_init_append(msg, &msgIt);
    /* Get the absolute index */
    argIdx = lua_absindex(L, argIdx);

    for ( idx = 0; idx < nArgs; ++idx )
    {
        lua_pushvalue(L, argIdx + idx);

        if ( !l2dbus_dbusComputeSignature(L, -1, sigBuf, 0) ||
            !dbus_signature_validate(cdbus_stringBufferRaw(sigBuf), NULL))
        {
            cdbus_stringBufferUnref(sigBuf);
            luaL_error(L, "cannot convert arg #%d to D-Bus type", argIdx + idx);
        }
        else
        {
            dbus_message_iter_init_append(msg, &msgIt);
            dbus_signature_iter_init(&sigIt, cdbus_stringBufferRaw(sigBuf));

            /* We have to make a protected call here because
             * the signature buffer is on the heap and if an error
             * occurs we need a chance to free up the signature before
             * propagating the error to Lua.
             */
            lua_pushcfunction(L, l2dbus_transcodeMarshallAsTypeShim);
            lua_pushvalue(L, argIdx + idx);
            lua_pushlightuserdata(L, &msgIt);
            lua_pushlightuserdata(L, &sigIt);
            if ( 0 != lua_pcall(L, 3, 0, 0) )
            {
                /* Free the signature buffer before the error long jump */
                cdbus_stringBufferUnref(sigBuf);

                /* Propagate the Lua error */
                lua_error(L);
            }
        }
        lua_pop(L, 1);
        cdbus_stringBufferClear(sigBuf);
    }

    cdbus_stringBufferUnref(sigBuf);
}


/**
 * @brief Converts D-Bus message arguments to equivalent Lua arguments.
 *
 * This function unmarshalls D-Bus message arguments into Lua arguments which
 * are appended to a Lua array. If an error is encountered then this function
 * will throw an Lua error. A Lua array is returned on the Lua stack containing
 * all the decoded D-Bus arguments.
 *
 * @param [in] L            The Lua state.
 * @param [in] msg          The message containing the D-Bus arguments.
 */
int
l2dbus_transcodeDbusArgsToLuaArray
    (
    lua_State*      L,
    DBusMessage*    msg
    )
{
    DBusMessageIter iter;
    int tableIdx;
    int arrIdx = 1;

    if ( NULL == msg )
    {
        luaL_error(L, "D-Bus message is missing");
    }
    else
    {
        lua_newtable(L);
        tableIdx = lua_gettop(L);
        dbus_message_iter_init(msg, &iter);
        while ( dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_INVALID )
        {
            l2dbus_transcodeUnmarshall(L, &iter, tableIdx, &arrIdx);
            dbus_message_iter_next(&iter);
        }
    }

    return 1;
}


/**
 * @brief Converts D-Bus message arguments to equivalent Lua arguments and
 * leaves them on the Lua stack.
 *
 * This function unmarshalls D-Bus message arguments into Lua arguments which
 * are pushed on the Lua stack. If an error is encountered then this function
 * will throw an Lua error.
 *
 * @param [in] L            The Lua state.
 * @param [in] msg          The message containing the D-Bus arguments.
 */
int
l2dbus_transcodeDbusArgsToLua
    (
    lua_State*      L,
    DBusMessage*    msg
    )
{
    size_t  arrLen = 0;
    size_t  idx;
    int tableIdx;

    l2dbus_transcodeDbusArgsToLuaArray(L, msg);
    if ( LUA_TTABLE == lua_type(L, -1) )
    {
        tableIdx = lua_absindex(L, -1);
        arrLen = lua_rawlen(L, tableIdx);
        if ( !lua_checkstack(L, arrLen) )
        {
            luaL_error(L,
                "cannot grow Lua stack to hold D-Bus message arguments");
        }

        /* Push all the elements on the stack */
        for ( idx = 1; idx <= arrLen; ++idx )
        {
            lua_rawgeti(L, tableIdx, idx);
        }
    }

    return arrLen;
}


/**
 * @brief Creates a metatable for a D-Bus type wrapper class.
 */
int
l2dbus_transcodeCreateMetatable
    (
    lua_State*      L,
    l2dbus_TypeId   metaTypeId,
    const luaL_Reg* funcs,
    int             dbusTypeId
    )
{
    l2dbus_createMetatable(L, metaTypeId, funcs);
    lua_pushinteger(L, dbusTypeId);
    lua_setfield(L, -2, "__dbusTypeId");

    return 1;
}


/**
 * @brief Creates the DbusTypes sub-module.
 *
 * This function simulates opening the DbusTypes sub-module.
 *
 * @return Returns one Lua element on the Lua stack - the sub-module table.
 */
int
l2dbus_openTranscode
    (
    lua_State*  L
    )
{
    int idx;

    lua_newtable(L);

    for ( idx = 0;
        idx < sizeof(gDbusTypeRegistry) / sizeof(gDbusTypeRegistry[0]);
        ++idx )
    {
        /* Create the associated metatable */
        lua_pop(L, l2dbus_transcodeCreateMetatable(L,
                gDbusTypeRegistry[idx].metaTypeId,
                gDbusTypeRegistry[idx].metaFuncs,
                gDbusTypeRegistry[idx].dbusTypeId));

        /* Create a table for the named D-Bus type that
         * contains a single constructor (new) function.
         */
        lua_pushstring(L, gDbusTypeRegistry[idx].name);

        /* Create the table with the single ctor function */
        lua_createtable(L, 0, 1);
        lua_pushliteral(L, "new");
        lua_pushcfunction(L, gDbusTypeRegistry[idx].ctor);
        lua_settable(L, -3);

        /* Now add the ctor table to the D-Bus type table */
        lua_settable(L, -3);

        /*
         * At this point you can create a D-Bus type by
         * calling something like:
         *    DbusTypes.Int16.new(5)
         */
    }

    return 1;
}

