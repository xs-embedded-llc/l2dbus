CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0)
PROJECT( l2dbus  C )

# Add an additional modules path
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules")
set(FIND_LIBRARY_USE_LIB64_PATHS TRUE)

include(FindPkgConfig)

# Define the L2DBUS root directory
set(L2DBUS_ROOT_DIR ${PROJECT_SOURCE_DIR})
set(L2DBUS_OUTPUT_DIR "${L2DBUS_ROOT_DIR}/${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_BUILD_TYPE}")
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${L2DBUS_OUTPUT_DIR}/lib )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${L2DBUS_OUTPUT_DIR}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${L2DBUS_OUTPUT_DIR}/bin )

set(L2DBUS_MAJOR_VERSION 1)
set(L2DBUS_MINOR_VERSION 0)
set(L2DBUS_RELEASE_VERSION 0)
set(L2DBUS_VERSION_STRING ${L2DBUS_MAJOR_VERSION}.${L2DBUS_MINOR_VERSION}.${L2DBUS_RELEASE_VERSION})

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0007 OLD)
endif(COMMAND cmake_policy)

set(L2DBUS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB L2DBUS_SRC_FILES "${L2DBUS_SRC_DIR}/*.c")

add_library(L2DBUS_MODULE SHARED ${L2DBUS_SRC_FILES})
set_target_properties(L2DBUS_MODULE PROPERTIES OUTPUT_NAME l2dbus_core)
set_target_properties(L2DBUS_MODULE PROPERTIES PREFIX "")

if( NOT L2DBUS_NO_LIBEV_LOOP )
    set(L2DBUS_EV_SRC_FILES
            "${L2DBUS_SRC_DIR}/main-loop/l2dbus_main-loop-ev.c"
            "${L2DBUS_SRC_DIR}/main-loop/l2dbus_module.c"
            "${L2DBUS_SRC_DIR}/l2dbus_compat.c"
            "${L2DBUS_SRC_DIR}/l2dbus_types.c"
            "${L2DBUS_SRC_DIR}/l2dbus_alloc.c"
            "${L2DBUS_SRC_DIR}/l2dbus_util.c"
       )
    add_library(L2DBUS_EV_MODULE SHARED ${L2DBUS_EV_SRC_FILES})
    set_target_properties(L2DBUS_EV_MODULE PROPERTIES OUTPUT_NAME l2dbus_ev)
    set_target_properties(L2DBUS_EV_MODULE PROPERTIES PREFIX "")
endif( NOT L2DBUS_NO_LIBEV_LOOP )

if( NOT L2DBUS_NO_GLIB_LOOP )
    set(L2DBUS_GLIB_SRC_FILES
            "${L2DBUS_SRC_DIR}/main-loop/l2dbus_main-loop-glib.c"
            "${L2DBUS_SRC_DIR}/main-loop/l2dbus_module.c"
            "${L2DBUS_SRC_DIR}/l2dbus_compat.c"
            "${L2DBUS_SRC_DIR}/l2dbus_types.c"
            "${L2DBUS_SRC_DIR}/l2dbus_alloc.c"
            "${L2DBUS_SRC_DIR}/l2dbus_util.c"
       )
    add_library(L2DBUS_GLIB_MODULE SHARED ${L2DBUS_GLIB_SRC_FILES})
    set_target_properties(L2DBUS_GLIB_MODULE PROPERTIES OUTPUT_NAME l2dbus_glib)
    set_target_properties(L2DBUS_GLIB_MODULE PROPERTIES PREFIX "")
endif( NOT L2DBUS_NO_GLIB_LOOP )


# Configure the compiler settings
include(CompilerSettings)

pkg_check_modules(DBUSLIB_PKG REQUIRED "dbus-1>=1.4.0")
include_directories(${DBUSLIB_PKG_INCLUDE_DIRS})
link_directories(${DBUSLIB_PKG_LIBRARY_DIRS})


if( NOT L2DBUS_NO_LIBEV_LOOP )
    pkg_check_modules(LIBEV_PKG "ev")
    if( LIBEV_PKG_FOUND )
        include_directories(${LIBEV_PKG_INCLUDE_DIRS})
        link_directories(${LIBEV_PKG_LIBRARY_DIRS})
    else( LIBEV_PKG_FOUND )
        # Search for the library in the usual places
        find_library( LIBEV_LIB ev )
        if( "${LIBEV_LIB}" STREQUAL "LIBEV_LIB-NOTFOUND" )
            message(FATAL_ERROR "Cannot find libev in the library search path")
        else( "${LIBEV_LIB}" STREQUAL "LIBEV_LIB-NOTFOUND" )
            find_path(LIBEV_PKG_INCLUDE_DIRS ev.h)
            include_directories(${LIBEV_PKG_INCLUDE_DIRS})
            get_filename_component(LIBEV_PKG_LIBRARY_DIRS ${LIBEV_LIB} PATH)
            link_directories(${LIBEV_PKG_LIBRARY_DIRS})
            set(LIBEV_PKG_LIBRARIES "ev")
            message(STATUS "Will link with libev found here: ${LIBEV_LIB}")
            message(STATUS "LIBEV_PKG_INCLUDE_DIRS: ${LIBEV_PKG_INCLUDE_DIRS}")
            message(STATUS "LIBEV_PKG_LIBRARY_DIRS: ${LIBEV_PKG_LIBRARY_DIRS}")
        endif( "${LIBEV_LIB}" STREQUAL "LIBEV_LIB-NOTFOUND" )
    endif( LIBEV_PKG_FOUND )
    
    pkg_check_modules(CDBUS_EV_PKG REQUIRED "cdbus-ev")
    include_directories(${CDBUS_EV_PKG_INCLUDE_DIRS})
    link_directories(${CDBUS_EV_PKG_LIBRARY_DIRS})
endif( NOT L2DBUS_NO_LIBEV_LOOP )

if( NOT L2DBUS_NO_GLIB_LOOP )
    pkg_check_modules(GLIB_PKG REQUIRED "glib-2.0>=2.0.0")
    include_directories(${GLIB_PKG_INCLUDE_DIRS})
    link_directories(${GLIB_PKG_LIBRARY_DIRS})

    pkg_check_modules(CDBUS_GLIB_PKG REQUIRED "cdbus-glib")
    include_directories(${CDBUS_GLIB_PKG_INCLUDE_DIRS})
    link_directories(${CDBUS_GLIB_PKG_LIBRARY_DIRS})
    
endif( NOT L2DBUS_NO_GLIB_LOOP )

pkg_check_modules(CDBUS_PKG "cdbus")
if( CDBUS_PKG_FOUND )
    include_directories(${CDBUS_PKG_INCLUDE_DIRS})
    link_directories(${CDBUS_PKG_LIBRARY_DIRS})
else( CDBUS_PKG_FOUND )
    # Search for the library in the usual places
    find_library( CDBUS_LIB "cdbus" )
    if( "${CDBUS_LIB}" STREQUAL "CDBUS_LIB-NOTFOUND" )
        message(FATAL_ERROR "Cannot find libcdbus in the library search path")
    else( "${CDBUS_LIB}" STREQUAL "CDBUS_LIB-NOTFOUND" )
        message(STATUS "Will link with libcdbus found here: "${CDBUS_LIB})
    endif( "${CDBUS_LIB}" STREQUAL "CDBUS_LIB-NOTFOUND" )
endif( CDBUS_PKG_FOUND )

# See if a specific Lua version has already been specified
if( NOT DEFINED L2DBUS_LUA_VERSION )
    # The default is to use 5.1
    set(L2DBUS_LUA_VERSION "5.1")
endif( NOT DEFINED L2DBUS_LUA_VERSION )

MESSAGE(STATUS "Searching for Lua ${L2DBUS_LUA_VERSION}")
find_package(Lua ${L2DBUS_LUA_VERSION} REQUIRED)
if ( "${LUA_VERSION_STRING}" STREQUAL "LUA_VERSION_STRING-NOTFOUND" )
    set(LUA_VERSION_STRING "${L2DBUS_LUA_VERSION}")
endif ( "${LUA_VERSION_STRING}" STREQUAL "LUA_VERSION_STRING-NOTFOUND" )

MESSAGE(STATUS "Found Lua version ${LUA_VERSION_STRING}")


include_directories("${LUA_INCLUDE_DIR}")
include_directories("${L2DBUS_SRC_DIR}")
target_link_libraries(L2DBUS_MODULE ${DBUSLIB_PKG_LIBRARIES}
                                    ${CDBUS_PKG_LIBRARIES}
                                    ${LUA_LIBRARIES})

if( NOT L2DBUS_NO_LIBEV_LOOP )
    target_link_libraries(L2DBUS_EV_MODULE
                                    ${CDBUS_PKG_LIBRARIES}
                                    ${CDBUS_EV_PKG_LIBRARIES}
                                    ${LUA_LIBRARIES})
endif( NOT L2DBUS_NO_LIBEV_LOOP )

if( NOT L2DBUS_NO_GLIB_LOOP )
    target_link_libraries(L2DBUS_GLIB_MODULE
                                    ${CDBUS_PKG_LIBRARIES}
                                    ${CDBUS_GLIB_PKG_LIBRARIES}
                                    ${LUA_LIBRARIES})
endif( NOT L2DBUS_NO_GLIB_LOOP )


LINK_DIRECTORIES(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# Installation setup
set(INSTALL_TARGETS_DEFAULT_ARGS
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION "lib/lua/${L2DBUS_LUA_VERSION}"
    ARCHIVE DESTINATION lib
)

IF(NOT DEFINED LUA_INSTALL_CMOD_PATH)
    SET(LUA_INSTALL_CMOD_PATH "lib/lua/${L2DBUS_LUA_VERSION}")
ENDIF(NOT DEFINED LUA_INSTALL_CMOD_PATH)

IF(NOT DEFINED LUA_INSTALL_LMOD_PATH)
    SET(LUA_INSTALL_LMOD_PATH "share/lua/${L2DBUS_LUA_VERSION}")
ENDIF(NOT DEFINED LUA_INSTALL_LMOD_PATH)

install(DIRECTORY "${L2DBUS_ROOT_DIR}/lua/l2dbus" DESTINATION "${LUA_INSTALL_LMOD_PATH}")
install(DIRECTORY "${L2DBUS_ROOT_DIR}/lua/ldbus" DESTINATION "${LUA_INSTALL_LMOD_PATH}")
install(TARGETS L2DBUS_MODULE DESTINATION "${LUA_INSTALL_CMOD_PATH}")

if( NOT L2DBUS_NO_LIBEV_LOOP )
    install(TARGETS L2DBUS_EV_MODULE DESTINATION "${LUA_INSTALL_CMOD_PATH}")
endif( NOT L2DBUS_NO_LIBEV_LOOP )

if( NOT L2DBUS_NO_GLIB_LOOP )
    install(TARGETS L2DBUS_GLIB_MODULE DESTINATION "${LUA_INSTALL_CMOD_PATH}")
endif( NOT L2DBUS_NO_GLIB_LOOP )

# Uninstall target
configure_file(
    "${PROJECT_SOURCE_DIR}/CMakeModules/CMakeUninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Documentation
FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/doxy.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/docs/doxy.cfg @ONLY)
  ADD_CUSTOM_TARGET(priv-docs ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/docs/doxy.cfg
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating private documentation (Doxygen)...")
ENDIF(DOXYGEN_FOUND)

IF(DEFINED LUA_EXECUTABLE)
  find_program(LDOC_EXECUTABLE ldoc.lua) 
  IF( "${LDOC_EXECUTABLE}" STREQUAL "LDOC_EXECUTABLE-NOTFOUND" )
    message(STATUS "ldoc not found - will not be able to generate public API docs.")  
  ELSE( "${LDOC_EXECUTABLE}" STREQUAL "LDOC_EXECUTABLE-NOTFOUND" )
    message(STATUS "Found LDoc: ${LDOC_EXECUTABLE}") 
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/config.ld.in ${CMAKE_CURRENT_BINARY_DIR}/docs/config.ld @ONLY)
    ADD_CUSTOM_TARGET(pub-docs ${LUA_EXECUTABLE} ${LDOC_EXECUTABLE} -c ${CMAKE_CURRENT_BINARY_DIR}/docs/config.ld .
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating public documentation (LDoc)...")
  ENDIF( "${LDOC_EXECUTABLE}" STREQUAL "LDOC_EXECUTABLE-NOTFOUND")
ENDIF(DEFINED LUA_EXECUTABLE)

# Make package release from source (must be done on the GIT source tree)
SET(L2DBUS_PKG_NAME "${PROJECT_NAME}-${L2DBUS_VERSION_STRING}.tar.gz")
SET(DIST_DIR "${CMAKE_BINARY_DIR}/dist-${PROJECT_NAME}")
ADD_CUSTOM_TARGET("dist-${PROJECT_NAME}"
    rm -rf "${DIST_DIR}" "${CMAKE_BINARY_DIR}/${L2DBUS_PKG_NAME}"
    COMMAND mkdir "${DIST_DIR}"
    COMMAND git clone --depth 1 ssh://git@git.xs-embedded.us:2222/l2dbus.git "${DIST_DIR}/${PROJECT_NAME}"
    COMMAND tar -czf "${CMAKE_BINARY_DIR}/${L2DBUS_PKG_NAME}"
            -C "${DIST_DIR}" --exclude-vcs --exclude="arm*" --exclude="x86*"
            --exclude=".cproject" --exclude=".project" --exclude=".settings"
            --exclude=".git" --exclude=".gitmodules" "${PROJECT_NAME}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    # Cleanup so that there is no copy in the source dir
    COMMAND rm -rf "${DIST_DIR}"
)


# Add a custom target to install/uninstall public documentation
ADD_CUSTOM_TARGET(install-docs
    install -d "${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}"
    COMMAND cp -rf "${CMAKE_CURRENT_BINARY_DIR}/docs/api/*" "${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
ADD_CUSTOM_TARGET(uninstall-docs
    rm -rf "${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
ADD_DEPENDENCIES(install-docs pub-docs)
ADD_DEPENDENCIES(uninstall uninstall-docs)


# Needs to be last statement:
INCLUDE(CPackSettings)

# Finished:
MESSAGE ("\n")
MESSAGE (STATUS "Compile ${PROJECT_NAME} using: make")
MESSAGE (STATUS "Install ${PROJECT_NAME} using: make install")
MESSAGE (STATUS "Uninstall ${PROJECT_NAME} using: make uninstall")
MESSAGE (STATUS "Compile public API documentation using: make pub-docs")
MESSAGE (STATUS "Compile private API  documentation using: make priv-docs")
MESSAGE (STATUS "Install public API  documentation using: make install-docs")
MESSAGE (STATUS "Uninstall public API  documentation using: make uninstall-docs")
MESSAGE (STATUS "Create ${PROJECT_NAME} source distribution using: make dist-${PROJECT_NAME}")
MESSAGE (STATUS " (be sure to set the correct CMAKE_INSTALL_PREFIX before)\n")



