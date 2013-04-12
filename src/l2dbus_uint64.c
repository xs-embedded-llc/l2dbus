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
 * @file           l2dbus_int64.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of Lua int64 type.
 *===========================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include "l2dbus_compat.h"
#include "l2dbus_util.h"
#include "l2dbus_uint64.h"
#include "l2dbus_int64.h"
#include "l2dbus_trace.h"
#include "l2dbus_object.h"
#include "l2dbus_defs.h"

/**
 L2DBUS Uint64

 This section describes a Lua Uint64 type used to manipulate D-Bus Uint64 types
 in the Lua environment.

 Since Lua's fundamental numerical type in most installations is a floating
 point number (a 'C' double or float), supporting D-Bus Uint64 types without
 loss of precision is problematic. As a result a Lua Uint64 type was created
 to wrap D-Bus Uint64 types and provide a means to manipulate and print these
 types from Lua. Likewise, these types can be added as arguments to D-Bus
 messages where they will be converted correctly to the underlying D-Bus
 Uint64 type.

 Several numerical operations are supported supported by this type. For
 binary operations the "other" number is first cast to an Uint64 before the
 operator is applied. What this means is that standard Lua numbers which are
 floating point will be truncated and potentially lose information. Please
 be aware of this limitation when operating on these types. The operators
 supported include:

 <ul>
 <li>Addition (+)</li>
 <li>Subtraction (-)</li>
 <li>Multiplication (*)</li>
 <li>Division (/) </li>
 <li>Modulus (%)</li>
 <li>Negation (-)</li>
 <li>Equal (==)</li>
 <li>Less Than (<)</li>
 <li>Less Than Equal (<=)</li>
 <li>Greater Than (>)</li>
 <li>Greater Than Equal (>=)</li>
 </ul>

 @namespace l2dbus.Uint64
 */



static uint64_t
l2dbus_uint64Cast
    (
    lua_State*  L,
    int         numIdx,
    int         baseIdx
    )
{
    uint64_t value;
    const char* vStr;
    l2dbus_Uint64* uint64Ud;
    l2dbus_Int64* int64Ud;
    int base;
    int numType = lua_type(L, numIdx);

    switch ( numType )
    {
        case LUA_TNUMBER:
            value = (uint64_t)lua_tonumber(L, numIdx);
            break;

        case LUA_TSTRING:
            base = l2dbus_isValidIndex(L, baseIdx) ? luaL_optint(L, baseIdx, 10) : 10;
            if ( ((2 > base) || (36 < base)) && (0 != base) )
            {
                luaL_error(L, "base must be range [2, 36] or equal to 0");
            }
            vStr = luaL_checkstring(L, numIdx);
            value = strtoull(vStr, NULL, base);
            if ( (0U == value) && ((errno == EINVAL) || (errno == ERANGE)) )
            {
                if ( EINVAL == errno )
                {
                    luaL_error(L, "unable to convert number");
                }
                else
                {
                    luaL_error(L, "number out of range");
                }
            }
            break;

        case LUA_TUSERDATA:
            uint64Ud = (l2dbus_Uint64*)l2dbus_isUserData(L, numIdx,
                                    L2DBUS_UINT64_MTBL_NAME);
            if ( NULL != uint64Ud )
            {
                value = uint64Ud->value;
            }
            else
            {
                int64Ud = (l2dbus_Int64*)l2dbus_isUserData(L, numIdx,
                                            L2DBUS_INT64_MTBL_NAME);
                if ( NULL != int64Ud )
                {
                    value = (uint64_t)int64Ud->value;
                }
                else
                {
                    luaL_error(L, "argument %d of type %s cannot be "
                                "converted to uint64", numIdx,
                                lua_typename(L, numType));
                }
            }
            break;

        default:
            luaL_error(L, "argument %d of type %s cannot be "
                       "converted to uint64", numIdx,
                       lua_typename(L, numType));
            break;
    }

    return value;
}


int
l2dbus_uint64Create
    (
    lua_State*  L,
    int         idx,
    int         base
    )
{
    uint64_t v;
    l2dbus_Uint64* ud;

    idx = lua_absindex(L, idx);
    lua_pushinteger(L, base);
    v = l2dbus_uint64Cast(L, idx, -1);

    /* Pop off the base */
    lua_pop(L, 1);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = v;
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Uint64 userdata=%p", ud));

    return 1;
}


/**
 @function new

 Creates a new Uint64 value.

 @tparam ?number|string value  The number to convert to an Uint64. If passed
 in as a string the <a href="http://www.manpagez.com/man/3/strtoul/">strtoul()</a>
 function is used to convert the number to a numerical value.

 @tparam ?number base The base must be in the range [2, 36] or equal to 0.
 See <a href="http://www.manpagez.com/man/3/strtoul/">strtoul()</a> for
 more details.
 @treturn userdata The userdata object representing an Uint64.
 */
static int
l2dbus_newUint64
    (
    lua_State*  L
    )
{
    int nArgs = lua_gettop(L);
    uint64_t v;
    l2dbus_Uint64* ud;

    switch ( nArgs )
    {
        case 0:
            v = 0U;
            break;

        case 1:
            /* Push the (default) base (10) to convert the string */
            lua_pushinteger(L, 10);
            v = l2dbus_uint64Cast(L, -2, -1);
            lua_pop(L, 1);
            break;

        default:
            v = l2dbus_uint64Cast(L, -2, -1);
            break;
    }

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = v;
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Uint64 userdata=%p", ud));

    return 1;
}


static int
l2dbus_uint64Add
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = a + b;
    return 1;
}


static int
l2dbus_uint64Subtract
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = a - b;
    return 1;
}


static int
l2dbus_uint64Multiply
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = a * b;
    return 1;
}


static int
l2dbus_uint64Divide
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = a / b;
    return 1;
}


static int
l2dbus_uint64Modulus
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = a % b;
    return 1;
}


static int
l2dbus_uint64Negate
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t n = -l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = n;
    return 1;
}


static int
l2dbus_uint64Power
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud;

    uint64_t base = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t exp = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);
    uint64_t result = 1;
    while ( exp != 0 )
    {
        if ( (exp & 1) == 1 )
        {
            result *= base;
        }
        exp >>= 1;
        base *= base;
    }

    ud = l2dbus_objectNew(L, sizeof(*ud), L2DBUS_UINT64_TYPE_ID);
    ud->value = result;
    return 1;
}


static int
l2dbus_uint64Equal
    (
    lua_State*  L
    )
{
    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);
    lua_pushboolean(L, a == b);
    return 1;
}


static int
l2dbus_uint64LessThan
    (
    lua_State*  L
    )
{
    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);
    lua_pushboolean(L, a < b);
    return 1;
}


static int
l2dbus_uint64LessEqual
    (
    lua_State*  L
    )
{
    uint64_t a = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    uint64_t b = l2dbus_uint64Cast(L, 2, L2DUS_INVALID_STACK_INDEX);
    lua_pushboolean(L, a <= b);
    return 1;
}


/**
 @function toNumber
 @within l2dbus.Uint64

 Converts the Uint64 to a Lua number.

 In converting the Uint64 to a Lua number there is the chance of losing
 precision since Lua number's typically cannot precisely represent all
 integral values. For a Lua double this range is [-2^52, 2^52 -1].

 @tparam userdata value The Uint64 value to convert to a Lua number.
 @treturn number A (possibly) equivalent Lua number.
 */
static int
l2dbus_uint64ToNumber
    (
    lua_State*  L
    )
{
    uint64_t v = l2dbus_uint64Cast(L, 1, L2DUS_INVALID_STACK_INDEX);
    lua_pushnumber(L, (lua_Number)v);
    return 1;
}


/**
 @function toString
 @within l2dbus.Uint64

 Converts the Uint64 to a string.

 @tparam userdata value The Uint64 value to convert to a string.
 @treturn string A string representing the Uint64.
 */
static int
l2dbus_uint64ToString
    (
    lua_State*  L
    )
{
    l2dbus_Uint64* ud = (l2dbus_Uint64*)luaL_checkudata(L, 1,
                        l2dbus_getNameByTypeId(L2DBUS_UINT64_TYPE_ID));
    int base = 10;
    int nArgs = lua_gettop(L);
    char fmt[16];
    char buf[64];

    if ( nArgs > 1 )
    {
        base = luaL_checkinteger(L, 2);
    }

    switch ( base )
    {
        case 8:
            sprintf(fmt, "%%"PRIo64);
            break;
        case 10:
            sprintf(fmt, "%%"PRIu64);
            break;
        case 16:
            sprintf(fmt, "0x%%"PRIx64);
            break;

        default:
            luaL_error(L, "arg #2 - unsupported base (%d)", base);
            break;
    }

    sprintf(buf, fmt, ud->value);
    lua_pushstring(L, buf);

    return 1;
}


static int
l2dbus_uint64Concat
    (
    lua_State*  L
    )
{
    l2dbus_getGlobalField(L, "tostring");
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    l2dbus_getGlobalField(L, "tostring");
    lua_pushvalue(L, 2);
    lua_call(L, 1, 1);
    lua_concat(L, 2);

    return 1;
}


/**
 * @brief Called by Lua VM to GC/reclaim the Uint64 userdata.
 *
 * This method is called by the Lua VM to reclaim the Uint64
 * userdata.
 *
 * @return nil
 *
 */
static int
l2dbus_uint64Dispose
    (
    lua_State*  L
    )
{
    /* Nothing to explicitly free */
    l2dbus_Uint64* ud = (l2dbus_Uint64*)luaL_checkudata(L, -1,
                        l2dbus_getNameByTypeId(L2DBUS_UINT64_TYPE_ID));

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: Uint64 (userdata=%p)", ud));
    return 0;
}


/*
 * Define the methods of the Uint64 class
 */
static const luaL_Reg l2dbus_uint64MetaTable[] = {
    {"__add", l2dbus_uint64Add},
    {"__sub", l2dbus_uint64Subtract},
    {"__mul", l2dbus_uint64Multiply},
    {"__div", l2dbus_uint64Divide},
    {"__mod", l2dbus_uint64Modulus},
    {"__unm", l2dbus_uint64Negate},
    {"__pow", l2dbus_uint64Power},
    {"__eq", l2dbus_uint64Equal},
    {"__lt", l2dbus_uint64LessThan},
    {"__le", l2dbus_uint64LessEqual},
    {"__len", l2dbus_uint64ToNumber},
    {"__tostring", l2dbus_uint64ToString},
    {"toString", l2dbus_uint64ToString},
    {"toNumber", l2dbus_uint64ToNumber},
    {"__concat", l2dbus_uint64Concat},
    {"__gc", l2dbus_uint64Dispose},
    {NULL, NULL},
};


/**
 * @brief Creates the Uint64 sub-module.
 *
 * This function creates a metatable entry for the Uint64 userdata
 * and simulates opening the Uint64 sub-module.
 *
 * @return A table defining the Uint64 sub-module
 *
 */
void
l2dbus_openUint64
    (
    lua_State*  L
    )
{
    /* Pop off the metatable */
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_UINT64_TYPE_ID,
            l2dbus_uint64MetaTable));

    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, l2dbus_newUint64);
    lua_setfield(L, -2, "new");
}



