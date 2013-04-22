#!/usr/bin/env bash

BUILD_TYPE="Release"
i=0
args=()
for arg in "$@"
do
    if [ "$arg" = "debug" -o "$arg" = "-DCMAKE_BUILD_TYPE=Debug" ]
    then
        BUILD_TYPE="Debug"
    elif [ "$arg" = "release" -o "$arg" = "-DCMAKE_BUILD_TYPE=Release" ]
    then
        BUILD_TYPE="Release"
    else
        args[$i]="$arg"
        ((++i))
    fi    
done

BUILD_TARGET=`uname -p`
BUILD_DIR="${BUILD_TARGET}-${BUILD_TYPE}"

if [ ! -d ${BUILD_DIR} ]; then
  mkdir ${BUILD_DIR}
fi

cd ${BUILD_DIR}

# To change the installation prefix pass in the following CMAKE definition:
# -DCMAKE_INSTALL_PREFIX=~/workspace/l2dbus/x86_64-Release/install

if [ ! -e Makefile ]; then
  cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE  "${args[@]}" ..
  if [ $? != 0 ]; then
    exit $_
  fi
fi

make

exit $?
