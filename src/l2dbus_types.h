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
 * @file           l2dbus_types.h        
 * @author         Glenn Schmottlach
 * @brief          Definition of common types
 *******************************************************************************
 */

#ifndef L2DBUS_TYPES_H_
#define L2DBUS_TYPES_H_

#ifndef L2DBUS_TRUE
#define L2DBUS_TRUE (1)
#endif

#ifndef L2DBUS_FALSE
#define L2DBUS_FALSE (0)
#endif

typedef int l2dbus_Bool;


#define L2DBUS_TYPE_TABLE \
X(L2DBUS_WATCH_TYPE_ID, L2DBUS_WATCH_MTBL_NAME) \
X(L2DBUS_TIMEOUT_TYPE_ID, L2DBUS_TIMEOUT_MTBL_NAME) \
X(L2DBUS_CONNECTION_TYPE_ID, L2DBUS_CONNECTION_MTBL_NAME) \
X(L2DBUS_DISPATCHER_TYPE_ID, L2DBUS_DISPATCHER_MTBL_NAME) \
X(L2DBUS_MESSAGE_TYPE_ID, L2DBUS_MESSAGE_MTBL_NAME) \
X(L2DBUS_PENDING_CALL_TYPE_ID, L2DBUS_PENDING_CALL_MTBL_NAME) \
X(L2DBUS_SERVICE_OBJECT_TYPE_ID, L2DBUS_SERVICE_OBJECT_MTBL_NAME) \
X(L2DBUS_INTERFACE_TYPE_ID, L2DBUS_INTERFACE_MTBL_NAME) \
X(L2DBUS_INT64_TYPE_ID, L2DBUS_INT64_MTBL_NAME) \
X(L2DBUS_UINT64_TYPE_ID, L2DBUS_UINT64_MTBL_NAME) \
\
X(L2DBUS_START_DBUS_TYPE_ID, L2DBUS_DBUS_START_MTBL_NAME) \
X(L2DBUS_DBUS_INVALID_TYPE_ID, L2DBUS_DBUS_INVALID_MTBL_NAME) \
X(L2DBUS_DBUS_BYTE_TYPE_ID, L2DBUS_DBUS_BYTE_MTBL_NAME) \
X(L2DBUS_DBUS_BOOLEAN_TYPE_ID, L2DBUS_DBUS_BOOLEAN_MTBL_NAME) \
X(L2DBUS_DBUS_INT16_TYPE_ID, L2DBUS_DBUS_INT16_MTBL_NAME) \
X(L2DBUS_DBUS_UINT16_TYPE_ID, L2DBUS_DBUS_UINT16_MTBL_NAME) \
X(L2DBUS_DBUS_INT32_TYPE_ID, L2DBUS_DBUS_INT32_MTBL_NAME) \
X(L2DBUS_DBUS_UINT32_TYPE_ID, L2DBUS_DBUS_UINT32_MTBL_NAME) \
X(L2DBUS_DBUS_INT64_TYPE_ID, L2DBUS_DBUS_INT64_MTBL_NAME) \
X(L2DBUS_DBUS_UINT64_TYPE_ID, L2DBUS_DBUS_UINT64_MTBL_NAME) \
X(L2DBUS_DBUS_DOUBLE_TYPE_ID, L2DBUS_DBUS_DOUBLE_MTBL_NAME) \
X(L2DBUS_DBUS_STRING_TYPE_ID, L2DBUS_DBUS_STRING_MTBL_NAME) \
X(L2DBUS_DBUS_OBJECT_PATH_TYPE_ID, L2DBUS_DBUS_OBJECT_PATH_MTBL_NAME) \
X(L2DBUS_DBUS_SIGNATURE_TYPE_ID, L2DBUS_DBUS_SIGNATURE_MTBL_NAME) \
X(L2DBUS_DBUS_ARRAY_TYPE_ID, L2DBUS_DBUS_ARRAY_MTBL_NAME) \
X(L2DBUS_DBUS_STRUCT_TYPE_ID, L2DBUS_DBUS_STRUCT_MTBL_NAME) \
X(L2DBUS_DBUS_VARIANT_TYPE_ID, L2DBUS_DBUS_VARIANT_MTBL_NAME) \
X(L2DBUS_DBUS_DICT_ENTRY_TYPE_ID, L2DBUS_DBUS_DICT_ENTRY_MTBL_NAME) \
X(L2DBUS_DBUS_UNIX_FD_TYPE_ID, L2DBUS_DBUS_UNIX_FD_MTBL_NAME) \
X(L2DBUS_END_DBUS_TYPE_ID, L2DBUS_DBUS_END_MTBL_NAME) \


#define X(a, b) a,
typedef enum
{
    L2DBUS_START_TYPE_ID = 0,
    L2DBUS_INVALID_TYPE_ID = L2DBUS_START_TYPE_ID,
    L2DBUS_TYPE_TABLE
    L2DBUS_END_TYPE_ID
} l2dbus_TypeId;
#undef X

#define X(a, b) extern const char b[];
L2DBUS_TYPE_TABLE
#undef X


const char* l2dbus_getNameByTypeId(l2dbus_TypeId typeId);

#endif /* Guard for L2DBUS_TYPES_H_ */
