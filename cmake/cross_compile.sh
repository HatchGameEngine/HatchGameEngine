#!/bin/bash

POSITIONAL_ARGS=()

BUILD_TYPE=Release
BUILD_ROOT=cross-windows

# Name of the output executable
EXECUTABLE_NAME=HatchGameEngine

# Game name
TARGET_NAME=HatchGameEngine

# Resource file
RESOURCE_FILE=meta/win/HatchGameEngine.rc

# Toggles source file compiling
ENABLE_SCRIPT_COMPILING=ON

# Toggles VM debugging
ENABLE_VM_DEBUGGING=OFF

# Generator used for CMake
CMAKE_GENERATOR="Unix Makefiles"

# Developer mode
DEVELOPER_MODE=OFF

# Portable mode
PORTABLE_MODE=OFF

# Allow loading .hatch resource files from the command line parameters
ALLOW_COMMAND_LINE_RESOURCE_LOAD=OFF

while [[ $# -gt 0 ]]; do
  case $1 in
    -b|--build-type) BUILD_TYPE="$2"; shift; shift ;;
    -r|--build-root) BUILD_ROOT="$2"; shift; shift ;;
    -e|--executable-name) EXECUTABLE_NAME="$2"; shift; shift ;;
    -t|--target-name) TARGET_NAME="$2"; shift; shift ;;
    -f|--resource-file) RESOURCE_FILE="$2"; shift; shift ;;
    -c|--disable-script-compiling) ENABLE_SCRIPT_COMPILING=OFF; shift ;;
    -g|--cmake-generator) CMAKE_GENERATOR="$2"; shift; shift ;;
    -v|--enable-vm-debugging) ENABLE_VM_DEBUGGING="$2"; shift; shift ;;
    -d|--developer-mode) DEVELOPER_MODE=ON; shift ;;
    -p|--portable-mode) PORTABLE_MODE=ON; shift ;;
    -h|--allow-command-line-resource-load) ALLOW_COMMAND_LINE_RESOURCE_LOAD=ON; shift ;;
    -*|--*) echo "Unknown option $1"; exit 1 ;;
    *) POSITIONAL_ARGS+=("$1"); shift ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}"

cmake  \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_TOOLCHAIN_FILE=cmake/WindowsToolchain.cmake \
  -DENABLE_SCRIPT_COMPILING=${ENABLE_SCRIPT_COMPILING} \
  -DENABLE_VM_DEBUGGING=${ENABLE_VM_DEBUGGING} \
  -DEXECUTABLE_NAME=${EXECUTABLE_NAME} \
  -DDEVELOPER_MODE=${DEVELOPER_MODE} \
  -DPORTABLE_MODE=${PORTABLE_MODE} \
  -DALLOW_COMMAND_LINE_RESOURCE_LOAD=${ALLOW_COMMAND_LINE_RESOURCE_LOAD} \
  -DTARGET_NAME=${TARGET_NAME} \
  -DRESOURCE_FILE=${RESOURCE_FILE} \
  -G "$CMAKE_GENERATOR" \
  -S .. -B ../builds/"${BUILD_ROOT}"
