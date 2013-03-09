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
