#!/bin/bash

POSITIONAL_ARGS=()

BUILD_TYPE=Release
BUILD_ROOT=cross-windows

# Name of the output executable
EXECUTABLE_NAME=HatchGameEngine

# Game name
TARGET_NAME=HatchGameEngine

# Resource file
RESOURCE_FILE=meta/win/icon.rc

# Toggles source file compiling
ENABLE_SCRIPT_COMPILING=ON

# Generator used for CMake
CMAKE_GENERATOR="Unix Makefiles"

while [[ $# -gt 0 ]]; do
  case $1 in
    -b|--build-type) BUILD_TYPE="$2"; shift; shift ;;
    -r|--build-root) BUILD_ROOT="$2"; shift; shift ;;
    -e|--executable-name) EXECUTABLE_NAME="$2"; shift; shift ;;
    -t|--target-name) TARGET_NAME="$2"; shift; shift ;;
    -f|--resource-file) RESOURCE_FILE="$2"; shift; shift ;;
    -d|--disable-script-compiling) ENABLE_SCRIPT_COMPILING=OFF; shift ;;
    -g|--cmake-generator) CMAKE_GENERATOR="$2"; shift; shift ;;
    -*|--*) echo "Unknown option $1"; exit 1 ;;
    *) POSITIONAL_ARGS+=("$1"); shift ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}"

cmake  \
	-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
	-DCMAKE_TOOLCHAIN_FILE=cmake/WindowsToolchain.cmake \
	-DENABLE_SCRIPT_COMPILING=${ENABLE_SCRIPT_COMPILING} \
	-DEXECUTABLE_NAME=${EXECUTABLE_NAME} \
  -DTARGET_NAME=${TARGET_NAME} \
	-DRESOURCE_FILE=${RESOURCE_FILE} \
  -G "$CMAKE_GENERATOR" \
	-S .. -B ../builds/"${BUILD_ROOT}"
