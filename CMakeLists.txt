CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0)

AUX_SOURCE_DIRECTORY(src LIBSOURCES)

add_library(l2dbus SHARED ${LIBSOURCES})
set_target_properties(l2dbus PROPERTIES OUTPUT_NAME l2dbus_core)
set_target_properties(l2dbus PROPERTIES PREFIX "")


ADD_LIBRARY(cdbus_static STATIC IMPORTED)
set(CDBUS_PATH "/usr/lib")
IF(CMAKE_CROSSCOMPILING)
    set(CDBUS_PATH "${CMAKE_FIND_ROOT_PATH}/usr/lib")
ENDIF(CMAKE_CROSSCOMPILING)
SET_TARGET_PROPERTIES(cdbus_static PROPERTIES IMPORTED_LOCATION ${CDBUS_PATH}/cdbus.a)
                                         

IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")


# The SDK a little messed up at the moment
IF(DEFINED LUA_INCLUDE_DIR)
	INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIR})
	message("Lua include = ${LUA_INCLUDE_DIR}")
ELSE(DEFINED LUA_INCLUDE_DIR) 
  IF(CMAKE_CROSSCOMPILING)
    include_directories(${CMAKE_FIND_ROOT_PATH}/usr/include/lua5.1)
  ENDIF(CMAKE_CROSSCOMPILING)
ENDIF(DEFINED LUA_INCLUDE_DIR)

include(FindPkgConfig)
pkg_check_modules(DBUSLIB_PKG REQUIRED "dbus-1")
include_directories(${DBUSLIB_PKG_INCLUDE_DIRS})
link_directories(${DBUSLIB_PKG_LIBRARY_DIRS})
target_link_libraries(l2dbus ${DBUSLIB_PKG_LIBRARIES})

pkg_check_modules(LUA_PKG REQUIRED "lua5.1")
include_directories(${LUA_PKG_INCLUDE_DIRS})

target_link_libraries(l2dbus cdbus_static ev m )

get_property(inc_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
message("Include dirs = ${inc_dirs}")

IF(NOT DEFINED INSTALL_LIBS)
	SET(INSTALL_LIBS "/usr/lib")
ENDIF(NOT DEFINED INSTALL_LIBS)

install(TARGETS l2dbus DESTINATION ${INSTALL_LIBS})
