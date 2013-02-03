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
 * @file           l2dbus_match.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus "match" object
 *******************************************************************************
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "l2dbus_compat.h"
#include "l2dbus_match.h"
#include "l2dbus_connection.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_message.h"
#include "l2dbus_alloc.h"
#include "lualib.h"


static void
l2dbus_matchHandler
    (
    cdbus_Connection*   conn,
    cdbus_Handle        hnd,
    DBusMessage*        msg,
    void*               userData
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    l2dbus_Match* match = (l2dbus_Match*)userData;

    assert( NULL != L );

    if ( NULL != match)
    {
        /* Push function and user value on the stack and execute the callback */
        lua_rawgeti(L, LUA_REGISTRYINDEX, match->cbCtx.funcRef);
        lua_pushlightuserdata(L, match);

        /* Leaves a Message userdata object on the stack */
        l2dbus_messageWrap(L, msg, L2DBUS_FALSE);

        lua_rawgeti(L, LUA_REGISTRYINDEX, match->cbCtx.userRef);

        if ( 0 != lua_pcall(L, 3 /* nArgs */, 0, 0) )
        {
            if ( lua_isstring(L, -1) )
            {
                errMsg = lua_tostring(L, -1);
            }
            L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Match callback error: %s", errMsg));
        }
    }

    /* Clean up the thread stack */
    lua_settop(L, 0);
}


static void
l2dbus_matchFreeRule
    (
    cdbus_MatchRule*    rule
    )
{
    int idx;
    if ( NULL != rule )
    {
        l2dbus_free(rule->member);
        l2dbus_free(rule->objInterface);
        l2dbus_free(rule->sender);
        l2dbus_free(rule->path);
        l2dbus_free(rule->localObjPath);
        l2dbus_free(rule->arg0Namespace);

        if ( NULL != rule->filterArgs )
        {
            for ( idx = 0; idx <= DBUS_MAXIMUM_MATCH_RULE_ARG_NUMBER; ++idx )
            {
                if ( CDBUS_FILTER_ARG_INVALID ==
                    rule->filterArgs[idx].argType)
                {
                    break;
                }
                l2dbus_free(rule->filterArgs[idx].value);
            }

            l2dbus_free(rule->filterArgs);
        }
    }
}


l2dbus_Match*
l2dbus_newMatch
    (
    lua_State*      L,
    int             ruleIdx,
    int             funcIdx,
    int             userIdx,
    int             connIdx,
    const char**    errMsg
    )
{
    l2dbus_Match* match = NULL;
    cdbus_MatchRule rule;
    int nFilterArgs = 0;
    int idx;
    int argArrayRef;
    int itemRef;
    int top;
    const char* reason = "";
    l2dbus_Bool failed = FALSE;
    cdbus_FilterArgType argType;
    l2dbus_Connection* connUd;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: match"));
    ruleIdx = lua_absindex(L, ruleIdx);
    funcIdx = lua_absindex(L, funcIdx);
    connIdx = lua_absindex(L, connIdx);

    /* Zero it out in preparation to filling it in */
    memset(&rule, 0, sizeof(rule));

    lua_getfield(L, ruleIdx, "msgType");
    if ( !lua_isnumber(L, -1) )
    {
        rule.msgType = CDBUS_MATCH_MSG_ANY;
    }
    else
    {
        switch ( lua_tointeger(L, -1) )
        {
            case DBUS_MESSAGE_TYPE_METHOD_CALL:
                rule.msgType = CDBUS_MATCH_MSG_METHOD_CALL;
                break;

            case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                rule.msgType = CDBUS_MATCH_MSG_METHOD_RETURN;
                break;

            case DBUS_MESSAGE_TYPE_ERROR:
                rule.msgType = CDBUS_MATCH_MSG_ERROR;
                break;

            case DBUS_MESSAGE_TYPE_SIGNAL:
                rule.msgType = CDBUS_MATCH_MSG_SIGNAL;
                break;

            default:
                rule.msgType = CDBUS_MATCH_MSG_ANY;
                break;
        }
    }
    /* Pop off the msgType */
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "member");
    if ( lua_isstring(L, -1) )
    {
        rule.member = l2dbus_strDup(lua_tostring(L, -1));
    }
    else
    {
        rule.member = NULL;
    }
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "interface");
    if ( lua_isstring(L, -1) )
    {
        rule.objInterface = l2dbus_strDup(lua_tostring(L, -1));
    }
    else
    {
        rule.objInterface = NULL;
    }
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "sender");
    if ( lua_isstring(L, -1) )
    {
        rule.sender = l2dbus_strDup(lua_tostring(L, -1));
    }
    else
    {
        rule.sender = NULL;
    }
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "path");
    if ( lua_isstring(L, -1) )
    {
        rule.path = l2dbus_strDup(lua_tostring(L, -1));
    }
    else
    {
        rule.path = NULL;
    }
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "treatPathAsNamespace");
    if ( lua_isboolean(L, -1) )
    {
        rule.treatPathAsNamespace = (lua_toboolean(L, -1) == 0) ? CDBUS_FALSE : CDBUS_TRUE;
    }
    else
    {
        rule.treatPathAsNamespace = CDBUS_FALSE;
    }
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "localObjPath");
    if ( lua_isstring(L, -1) )
    {
        rule.localObjPath = l2dbus_strDup(lua_tostring(L, -1));
    }
    else
    {
        rule.localObjPath = NULL;
    }
    lua_pop(L, 1);


    lua_getfield(L, ruleIdx, "arg0Namespace");
    if ( lua_isstring(L, -1) )
    {
        rule.arg0Namespace = l2dbus_strDup(lua_tostring(L, -1));
    }
    else
    {
        rule.arg0Namespace = NULL;
    }
    lua_pop(L, 1);

    lua_getfield(L, ruleIdx, "eavesdrop");
    if ( lua_isboolean(L, -1) )
    {
        rule.eavesdrop = (lua_toboolean(L, -1) == 0) ? CDBUS_FALSE : CDBUS_TRUE;
    }
    else
    {
        rule.eavesdrop = CDBUS_FALSE;
    }
    lua_pop(L, 1);

    rule.filterArgs = NULL;
    lua_getfield(L, ruleIdx, "filterArgs");
    if ( lua_istable(L, -1) )
    {
        argArrayRef = lua_absindex(L, -1);
        nFilterArgs = lua_objlen(L, -1);
        if ( nFilterArgs > 0 )
        {
            if ( nFilterArgs > DBUS_MAXIMUM_MATCH_RULE_ARG_NUMBER+1 )
            {
                nFilterArgs = DBUS_MAXIMUM_MATCH_RULE_ARG_NUMBER+1;
            }
            rule.filterArgs = (cdbus_FilterArgItem*)l2dbus_calloc(nFilterArgs+1,
                                sizeof(cdbus_FilterArgItem));
            if ( NULL == rule.filterArgs )
            {
                failed = TRUE;
                reason = "failed to allocate memory for argN filter elements";
            }
            else
            {
                /* Remember the top of the stack in case we need
                 * to break out of the loop. This will be used
                 * to "restore" the stack to it's previous level.
                 */
                top = lua_gettop(L);

                /* Loop over all the argN filters */
                for ( idx = 0; idx < nFilterArgs; ++idx )
                {
                    lua_rawgeti(L, argArrayRef, idx+1);
                    if ( !lua_istable(L, -1) )
                    {
                        failed = L2DBUS_TRUE;
                        reason = "argN table expected";
                        break;
                    }

                    itemRef = lua_absindex(L, -1);

                    argType = CDBUS_FILTER_ARG_INVALID;
                    lua_getfield(L, itemRef, "type");
                    if ( lua_isstring(L, -1) )
                    {
                        if ( 0 == strcmp(lua_tostring(L, -1), "string") )
                        {
                            argType = CDBUS_FILTER_ARG;
                        }
                        else if ( 0 == strcmp(lua_tostring(L, -1), "path") )
                        {
                            argType = CDBUS_FILTER_ARG_PATH;
                        }
                    }
                    /* Else if this field was *not* specified then */
                    else if ( lua_isnil(L, -1) )
                    {
                        /* The default is to treat the argument as a
                         * regular (string) argument for the matching.
                         */
                        argType = CDBUS_FILTER_ARG;
                    }
                    lua_pop(L, 1);

                    if ( (CDBUS_FILTER_ARG != argType) &&
                        (CDBUS_FILTER_ARG_PATH != argType) )
                    {
                        failed = L2DBUS_TRUE;
                        reason = "unknown argument type specified (!= 'path' or 'string')";
                        break;
                    }
                    rule.filterArgs[idx].argType = argType;

                    lua_getfield(L, itemRef, "index");
                    if ( !lua_isnumber(L, -1) )
                    {
                        failed = L2DBUS_TRUE;
                        reason = "arg filter index not specified";
                        break;
                    }
                    rule.filterArgs[idx].argN = lua_tointeger(L, -1);
                    lua_pop(L, 1);
                    /* If the argN index is out of range then ... */
                    if ( (0 > rule.filterArgs[idx].argN) ||
                        (DBUS_MAXIMUM_MATCH_RULE_ARG_NUMBER <
                         rule.filterArgs[idx].argN) )
                    {
                        failed = L2DBUS_TRUE;
                        reason = "arg filter index out of range";
                        break;
                    }

                    lua_getfield(L, itemRef, "value");
                    if ( !lua_isstring(L, -1) )
                    {
                        failed = L2DBUS_TRUE;
                        reason = "arg filter missing a value";
                        break;
                    }

                    rule.filterArgs[idx].value = l2dbus_strDup(lua_tostring(L, -1));

                    /* Pop the value and the item table */
                    lua_pop(L, 2);
                }

                /* Restore the top of the stack */
                lua_settop(L, top);

                /* Set a marker to indicate the end of the array */
                rule.filterArgs[idx].argType = CDBUS_FILTER_ARG_INVALID;
            }
        }
    }

    /* Pop what should be the filterArgs table */
    lua_pop(L, 1);

    /* If the rule has been parsed successfully then ... */
    if ( !failed )
    {
        match = (l2dbus_Match*)l2dbus_calloc(1, sizeof(*match));
        if ( NULL == match )
        {
            failed = L2DBUS_TRUE;
            reason = "failed to allocate memory for match object";
        }
        else
        {
            connUd = (l2dbus_Connection*)lua_touserdata(L, connIdx);
            match->matchHnd = cdbus_connectionRegMatchHandler(
                                                    connUd->conn,
                                                    l2dbus_matchHandler,
                                                    match,
                                                    &rule,
                                                    NULL);

            if ( CDBUS_INVALID_HANDLE == match->matchHnd )
            {
                failed = L2DBUS_TRUE;
                reason = "failed to register match handler";
            }
            else
            {
                lua_pushvalue(L, connIdx);
                match->connRef = luaL_ref(L, LUA_REGISTRYINDEX);
                l2dbus_callbackInit(&match->cbCtx);
                l2dbus_callbackRef(L, funcIdx, userIdx, &match->cbCtx);
            }
        }
    }

    if ( *errMsg != NULL )
    {
        *errMsg = reason;
    }

    if ( failed )
    {
        free(match);
        match = NULL;
    }

    /* Always free the rule since we no longer need it */
    l2dbus_matchFreeRule(&rule);

    return match;
}


void
l2dbus_disposeMatch
    (
    lua_State*      L,
    l2dbus_Match*   match
    )
{
    cdbus_HResult rc;
    l2dbus_Connection* connUd;

    if ( NULL != match )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, match->connRef);
        connUd = (l2dbus_Connection*)lua_touserdata(L, -1);
        rc = cdbus_connectionUnregMatchHandler(connUd->conn, match->matchHnd);
        if ( CDBUS_FAILED(rc) )
        {
            L2DBUS_TRACE((L2DBUS_TRC_WARN, "Failed to unregister match (0x%x)", rc));
        }
        l2dbus_callbackUnref(L, &match->cbCtx);
        /* Pop of the connection userdata */
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, match->connRef);
        l2dbus_free(match);
    }
}


