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
 * @file           l2dbus_core.h
 * @author         Glenn Schmottlach
 * @brief          Global definitions for the module.
 *******************************************************************************
 */

#ifndef L2DBUS_CORE_H_

/* Forward Declarations */
struct lua_State;


#define L2DBUS_VERSION_MAJOR    "1"
#define L2DBUS_VERSION_MINOR    "0"
#define L2DBUS_VERSION_RELEASE  "0"
#define L2DBUS_VERSION_NUMBER   010000
#define L2DBUS_VERSION_STRING   L2DBUS_VERSION_MAJOR "." \
                                L2DBUS_VERSION_MINOR "." \
                                L2DBUS_VERSION_RELEASE
#define L2DBUS_COPYRIGHT        "(c) Copyright 2013 XS-Embedded LLC"
#define L2DBUS_AUTHOR           "Glenn Schmottlach"

void l2dbus_checkModuleInitialized(struct lua_State* L);
int l2dbus_moduleFinalizerRef(struct lua_State* L);
void l2dbus_moduleFinalizerUnref(struct lua_State* L, int ref);


#define L2DBUS_CORE_H_



#endif /* Guard for L2DBUS_CORE_H_ */
