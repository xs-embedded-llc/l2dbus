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
 * @file           l2dbus_match.h
 * @author         Glenn Schmottlach
 * @brief          Definition of the CDBUS "match" object
 *===========================================================================
 */

#ifndef L2DBUS_MATCH_H_
#define L2DBUS_MATCH_H_

#include "lua.h"
#include "queue.h"
#include "cdbus/cdbus.h"
#include "l2dbus_callback.h"

/* Forward declarations */
struct cdbus_MatchRule;

typedef struct l2dbus_Match
{
    int                         connRef;
    l2dbus_CallbackCtx          cbCtx;
    cdbus_Handle                matchHnd;
    LIST_ENTRY(l2dbus_Match)    link;
} l2dbus_Match;

l2dbus_Match* l2dbus_newMatch(lua_State* L, int ruleIdx, int funcIdx, int userIdx,
                                int connIdx, const char** errMsg);
void l2dbus_disposeMatch(lua_State* L, l2dbus_Match* match);

#endif /* Guard for L2DBUS_MATCH_H_ */
