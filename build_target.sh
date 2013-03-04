#!/bin/sh

BUILD_TYPE="Release"
if [ "$1" == "debug" ]; then
    BUILD_TYPE="Debug" 
fi

# Suck in the environment settings for pkg-conf
. /usr/local/sdk-imx6/environment-setup

BUILD_DIR=armv7l

if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi

cd ${BUILD_DIR}

if [ ! -e Makefile ]; then
  cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_TOOLCHAIN_FILE=/usr/local/sdk-imx6/cmake.toolchain ..
  if [ $? != 0 ]; then
    exit $_
  fi
fi

make

exit $?
