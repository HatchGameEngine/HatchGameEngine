cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(HatchGameEngine VERSION 1.4.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set up CMake modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

file(GLOB_RECURSE C_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/*.c")
file(GLOB_RECURSE CPP_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/*.cpp")

set(HATCH_SOURCES ${C_SOURCES} ${CPP_SOURCES})

set(HATCH_TARGET_NAME ${PROJECT_NAME})
if(TARGET_NAME)
  set(HATCH_TARGET_NAME ${TARGET_NAME})
endif()

add_executable(${PROJECT_NAME} ${HATCH_SOURCES})

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(OUT_EXEC_NAME ${PROJECT_NAME}-${CMAKE_BUILD_TYPE})

# Change executable name
if(EXECUTABLE_NAME)
  set(OUT_EXEC_NAME ${EXECUTABLE_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${OUT_EXEC_NAME}")

add_definitions(-DTARGET_NAME=\"${HATCH_TARGET_NAME}\")

set(CONSOLE_BASE_PATH "" CACHE STRING "Optional base path (must end with '/')")
if(NOT CONSOLE_BASE_PATH STREQUAL "")
  add_definitions(-DCONSOLE_BASE_PATH="${CONSOLE_BASE_PATH}")
endif()

# Build options
option(ENABLE_SCRIPT_COMPILING "Enable script compiling" ON)
option(ENABLE_USING_VM_FUNCPTRS "Enable function-pointer based VM" ON)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  option(ENABLE_VM_DEBUGGING "Enable VM debugging" ON)
  option(DEVELOPER_MODE "Developer mode" ON)
else()
  option(ENABLE_VM_DEBUGGING "Enable VM debugging" OFF)
  option(DEVELOPER_MODE "Developer mode" OFF)
endif()

option(PORTABLE_MODE "Portable mode" OFF)
option(ALLOW_COMMAND_LINE_RESOURCE_LOAD "Allow loading resource files from command line" ON)

option(USE_OPEN_ASSET_IMPORT_LIBRARY "Use Open Asset Import Library" ON)

option(USE_DEFAULT_FONTS "Build with default fonts" ON)

# Renderers
option(USING_OPENGL "Use OpenGL" ON)

if(USING_OPENGL)
  add_definitions(-DUSING_OPENGL)
endif()


if(NOT ENABLE_SCRIPT_COMPILING)
  add_definitions(-DNO_SCRIPT_COMPILING)
endif()

if(ENABLE_USING_VM_FUNCPTRS)
  add_definitions(-DUSING_VM_FUNCPTRS)
endif()

if(ENABLE_VM_DEBUGGING)
  add_definitions(-DVM_DEBUG)
endif()

if(DEVELOPER_MODE)
  add_definitions(-DDEVELOPER_MODE)
endif()

if(PORTABLE_MODE)
  add_definitions(-DPORTABLE_MODE)
endif()

if(ALLOW_COMMAND_LINE_RESOURCE_LOAD)
  add_definitions(-DALLOW_COMMAND_LINE_RESOURCE_LOAD)
endif()

if(USE_DEFAULT_FONTS)
  add_definitions(-DUSE_DEFAULT_FONTS)
endif()

add_definitions(-DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME)

find_file(REPO_DIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)

if (REPO_DIR)
  execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  add_definitions("-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
endif()

# Find platform cmake file
find_file(PLATFORM_FILE NAMES "${CMAKE_SYSTEM_NAME}.cmake" PATHS "${CMAKE_SOURCE_DIR}/cmake/Platforms" NO_DEFAULT_PATH)
if(NOT PLATFORM_FILE)
  message(FATAL_ERROR "Could not find cmake file for target: '${CMAKE_SYSTEM_NAME}'")
endif()
include(${PLATFORM_FILE})

# Add needed directories
include_directories(
  include
  source
  ${SDL2_INCLUDE_DIRS}
)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  include_directories(${ASSIMP_INCLUDE_DIRS})
endif()

# Add libraries
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()

if(PNG_FOUND)
  add_definitions(-DUSING_LIBPNG)
  target_link_libraries(${PROJECT_NAME} ${PNG_LIBRARIES})
endif()

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  if(assimp_FOUND)
    add_definitions(-DUSING_ASSIMP)
    target_link_libraries(${PROJECT_NAME} ${ASSIMP_LIBRARIES})
  endif()
endif()

if(USING_OPENGL)
  target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()
