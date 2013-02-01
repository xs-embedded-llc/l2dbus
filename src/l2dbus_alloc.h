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
 * @file           l2dbus_alloc.h
 * @author         Glenn Schmottlach
 * @brief          Declarations of memory allocation routines.
 *******************************************************************************
 */

#ifndef L2DBUS_ALLOC_H_
#define L2DBUS_ALLOC_H_

#include <stddef.h>

void* l2dbus_malloc(size_t size);
void* l2dbus_calloc(size_t numElt, size_t eltSize);
void* l2dbus_realloc(void* memory, size_t bytes);
void l2dbus_free(void* p);
void l2dbus_freeStringArray(char** strArray);
char* l2dbus_strDup(const char* s);

#endif /* Guard for L2DBUS_ALLOC_H_ */
