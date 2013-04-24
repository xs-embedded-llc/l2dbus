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
 * @file           l2dbus_reflist.h
 * @author         Glenn Schmottlach
 * @brief          Definition of an object reference list.
 *===========================================================================
 */

#ifndef L2DBUS_REFLIST_H_
#define L2DBUS_REFLIST_H_
#include "lua.h"
#include "queue.h"
#include "l2dbus_types.h"

typedef struct l2dbus_RefItem
{
    int refIdx;
    LIST_ENTRY(l2dbus_RefItem) link;
} l2dbus_RefItem;


typedef struct l2dbus_RefList
{
    LIST_HEAD(l2dbus_RefListHead,
                l2dbus_RefItem)     list;
} l2dbus_RefList;

typedef struct l2dbus_RefListIter
{
    l2dbus_RefItem* cur;
    l2dbus_RefItem* next;
} l2dbus_RefListIter;


typedef void (*l2dbus_FreeItemFunc)(void* item, void* userdata);

void l2dbus_refListInit(l2dbus_RefList* refList);
void l2dbus_refListFree(l2dbus_RefList* refList, lua_State* L,
                        l2dbus_FreeItemFunc func, void* userdata);

int l2dbus_refListRef(l2dbus_RefList* refList, lua_State* L, int idx);
l2dbus_Bool l2dbus_refListUnref(l2dbus_RefList* refList, lua_State* L, int ref);


void l2dbus_refListIterInit(l2dbus_RefList* refList, l2dbus_RefListIter* iter);
int l2dbus_refListIterCurrent(l2dbus_RefList* refList, l2dbus_RefListIter* iter);
l2dbus_Bool l2dbus_refListIterNext(l2dbus_RefList* refList, l2dbus_RefListIter* iter);
void l2dbus_refListIterErase(l2dbus_RefList* refList, lua_State* L, l2dbus_RefListIter* iter);
void* l2dbus_refListIterRefItem(l2dbus_RefList* refList, lua_State* L, l2dbus_RefListIter* iter);
l2dbus_Bool l2dbus_refListFindItem(l2dbus_RefList* refList, lua_State* L, const void* item, l2dbus_RefListIter* iter);








#endif /* Guard for L2DBUS_OBJECT_H_ */
