#!/bin/sh

BUILD_TYPE="Release"
if [ "$1" == "debug" ]; then
    BUILD_TYPE="Debug" 
fi

BUILD_DIR=`uname -p`

if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi

cd ${BUILD_DIR}

if [ ! -e Makefile ]; then
  cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE  ..
  if [ $? != 0 ]; then
    exit $_
  fi
fi

make

exit $?
