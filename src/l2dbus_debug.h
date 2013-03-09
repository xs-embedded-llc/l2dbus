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
 * @file           l2dbus_debug.h        
 * @author         Glenn Schmottlach
 * @brief          Definition of basic debug routines.
 *===========================================================================
 */

#ifndef L2DBUS_DEBUG_H_
#define L2DBUS_DEBUG_H_

/* Forward Definitions */
struct lua_State;

void l2dbus_dumpItem(struct lua_State* L, int idx, const char* prefix);
void l2dbus_dumpTable(struct lua_State* L, int tableIdx, const char* name);
void l2dbus_dumpUserData(struct lua_State* L, int udIdx, const char* prefix);
void l2dbus_dumpStack(struct lua_State* L);


#ifdef TRACE
    #define L2DBUS_DUMPSTACK(L) l2dbus_dumpStack(L)
#else
    #define L2DBUS_DUMPSTACK(L) do { if ( 0 ) l2dbus_dumpStack(L); } while ( 0 )
#endif  /* TRACE */

#endif /* Guard for L2DBUS_DEBUG_H_ */
