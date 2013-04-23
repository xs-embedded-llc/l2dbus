#!/usr/bin/env bash

BUILD_TYPE="Release"

i=0
args=()
for arg in "$@"
do
    if [ "$arg" = "debug" ]
    then
        BUILD_TYPE="Debug"
    elif [ "$arg" = "release" ]
    then
        BUILD_TYPE="Release"
    elif [ "${arg:0:19}" = "-DCMAKE_BUILD_TYPE=" ]
    then
        BUILD_TYPE="${arg:19}"
    else
        args[$i]="$arg"
        ((++i))
    fi    
done

# Set some defaults
if [ -d /usr/local/sdk-imx6 ]; then
    CMAKE_TOOLCHAIN_PATH_PREFIX="/usr/local/sdk-imx6"
else
    CMAKE_TOOLCHAIN_PATH_PREFIX="$PWD"
fi



# Set up the environment for the build tools
if [ -e "${CMAKE_TOOLCHAIN_PATH_PREFIX}/environment-setup" ]; then
    . "${CMAKE_TOOLCHAIN_PATH_PREFIX}/environment-setup"
else
    echo -e "Target environment may not be set up properly!\n"
fi

if [ ! -e "${CMAKE_TOOLCHAIN_PATH_PREFIX}/cmake.toolchain" ]; then
   echo "cmake.toolchain file is not found!"
   exit 1 
fi

# Determine the build target
BUILD_TARGET=$(cat "${CMAKE_TOOLCHAIN_PATH_PREFIX}/cmake.toolchain" | \
                perl -n -e'/CMAKE_SYSTEM_PROCESSOR\s+(\w+)\)/ && print $1')
if [ "${BUILD_TARGET}" = "" ]; then
    BUILD_TARGET=armv7l
    echo "Build target not found - assuming default (${BUILD_TARGET})"
fi

BUILD_DIR="${BUILD_TARGET}-${BUILD_TYPE}"

echo -e "==> Running cmake for ${BUILD_DIR} . . .\n"


if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi

BUILD_DIR_PATH=$(dirname ${BUILD_DIR})

cd ${BUILD_DIR}

if [ ! -e Makefile ]; then
  cmake -DCMAKE_SYSTEM_PROCESSOR=${BUILD_TARGET} \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_PATH_PREFIX}/cmake.toolchain \
        "${args[@]}" ..
  if [ $? != 0 ]; then
    exit $_
  fi
fi

make

exit $?
