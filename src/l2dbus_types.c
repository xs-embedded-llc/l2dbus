/*******************************************************************************
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
 *******************************************************************************
 *******************************************************************************
 * @file           l2dbus_types.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of module types.
 *******************************************************************************
 */
#include <stdlib.h>
#include "l2dbus_types.h"
#include "l2dbus_defs.h"

#define L2DBUS_MAKE_METANAME(N) L2DBUS_META_TABLE_PREFIX N

const char L2DBUS_MODULE_FINALIZER_MTBL_NAME[] = L2DBUS_MAKE_METANAME("module_finalizer");
const char L2DBUS_WATCH_MTBL_NAME[] = L2DBUS_MAKE_METANAME("watch");
const char L2DBUS_TIMEOUT_MTBL_NAME[] = L2DBUS_MAKE_METANAME("timeout");
const char L2DBUS_CONNECTION_MTBL_NAME[] = L2DBUS_MAKE_METANAME("connection");
const char L2DBUS_DISPATCHER_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dispatcher");
const char L2DBUS_MESSAGE_MTBL_NAME[] = L2DBUS_MAKE_METANAME("message");
const char L2DBUS_PENDING_CALL_MTBL_NAME[] = L2DBUS_MAKE_METANAME("pending_call");
const char L2DBUS_SERVICE_OBJECT_MTBL_NAME[] = L2DBUS_MAKE_METANAME("service_object");
const char L2DBUS_INTERFACE_MTBL_NAME[] = L2DBUS_MAKE_METANAME("interface");
const char L2DBUS_INT64_MTBL_NAME[] = L2DBUS_MAKE_METANAME("int64");
const char L2DBUS_UINT64_MTBL_NAME[] = L2DBUS_MAKE_METANAME("uint64");

const char L2DBUS_DBUS_START_MTBL_NAME[] = "";
const char L2DBUS_DBUS_INVALID_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.invalid");
const char L2DBUS_DBUS_BYTE_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.byte");
const char L2DBUS_DBUS_BOOLEAN_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.boolean");
const char L2DBUS_DBUS_INT16_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.int16");
const char L2DBUS_DBUS_UINT16_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.uint16");
const char L2DBUS_DBUS_INT32_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.int32");
const char L2DBUS_DBUS_UINT32_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.uint32");
const char L2DBUS_DBUS_INT64_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.int64");
const char L2DBUS_DBUS_UINT64_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.uint64");
const char L2DBUS_DBUS_DOUBLE_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.double");
const char L2DBUS_DBUS_STRING_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.string");
const char L2DBUS_DBUS_OBJECT_PATH_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.object_path");
const char L2DBUS_DBUS_SIGNATURE_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.signature");
const char L2DBUS_DBUS_ARRAY_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.array");
const char L2DBUS_DBUS_STRUCT_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.structure");
const char L2DBUS_DBUS_VARIANT_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.variant");
const char L2DBUS_DBUS_DICT_ENTRY_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.dictionary");
const char L2DBUS_DBUS_UNIX_FD_MTBL_NAME[] = L2DBUS_MAKE_METANAME("dbus.unix_fd");
const char L2DBUS_DBUS_END_MTBL_NAME[] = "";

#define X(a, b) b,
const char* l2dbus_gMetaName[] = {
    L2DBUS_TYPE_TABLE
};
#undef X


const char*
l2dbus_getNameByTypeId
    (
    l2dbus_TypeId   typeId
    )
{
    const char* name = NULL;

    if ( (L2DBUS_START_TYPE_ID < typeId) && (L2DBUS_END_TYPE_ID > typeId))
    {
        name = l2dbus_gMetaName[(int)typeId - 1];
    }
    return name;
}

