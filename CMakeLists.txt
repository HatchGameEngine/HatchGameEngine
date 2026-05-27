cmake_minimum_required(VERSION 3.14)

# set the project name and version
project(HatchGameEngine VERSION 1.4.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set up CMake modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

file(GLOB_RECURSE C_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/Engine/*.c")
file(GLOB_RECURSE CPP_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/Engine/*.cpp")

set(LIBRARY_SOURCES
  source/Libraries/miniz.c
  source/Libraries/spng.c
  source/Libraries/stb_vorbis.c
)

list(APPEND LIBRARY_SOURCES
  source/Libraries/Clipper2/clipper.engine.cpp
  source/Libraries/Clipper2/clipper.offset.cpp
  source/Libraries/Clipper2/clipper.rectclip.cpp
)

list(APPEND LIBRARY_SOURCES
  source/Libraries/poly2tri/common/shapes.cpp
  source/Libraries/poly2tri/sweep/advancing_front.cpp
  source/Libraries/poly2tri/sweep/cdt.cpp
  source/Libraries/poly2tri/sweep/sweep.cpp
  source/Libraries/poly2tri/sweep/sweep_context.cpp
)

set(HATCH_TARGET_NAME ${PROJECT_NAME})
if(TARGET_NAME)
  set(HATCH_TARGET_NAME ${TARGET_NAME})
endif()

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
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

if(NOT NINTENDO_SWITCH)
  option(ENABLE_USING_LINENOISE "Use linenoise-ng for the script debugger command line interface" ON)
  if (ENABLE_USING_LINENOISE)
    list(APPEND LIBRARY_SOURCES
      source/Libraries/linenoise-ng/linenoise.cpp
      source/Libraries/linenoise-ng/wcwidth.cpp
    )
  endif()
endif()

option(PORTABLE_MODE "Portable mode" OFF)
option(ALLOW_COMMAND_LINE_RESOURCE_LOAD "Allow loading resource files from command line" ON)

option(USE_OPEN_ASSET_IMPORT_LIBRARY "Use Open Asset Import Library" ON)

option(USE_DEFAULT_FONTS "Build with default fonts" ON)

# Renderers
option(USING_OPENGL "Use OpenGL" ON)

# Set sources
set(HATCH_SOURCES ${C_SOURCES} ${CPP_SOURCES} ${LIBRARY_SOURCES})

set(OUT_EXEC_NAME ${PROJECT_NAME}-${CMAKE_BUILD_TYPE})

# Change executable name
if(EXECUTABLE_NAME)
  set(OUT_EXEC_NAME ${EXECUTABLE_NAME})
endif()

# Find platform cmake file
find_file(PLATFORM_FILE NAMES "${CMAKE_SYSTEM_NAME}.cmake" PATHS "${CMAKE_SOURCE_DIR}/cmake/Platforms" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT PLATFORM_FILE)
  message(FATAL_ERROR "Could not find cmake file for target: '${CMAKE_SYSTEM_NAME}'")
endif()
include(${PLATFORM_FILE})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${OUT_EXEC_NAME}")

if(USING_OPENGL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_OPENGL)
endif()

if(NOT ENABLE_SCRIPT_COMPILING)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DNO_SCRIPT_COMPILING)
endif()

if(ENABLE_USING_VM_FUNCPTRS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_VM_FUNCPTRS)
endif()

if(ENABLE_VM_DEBUGGING)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DVM_DEBUG)

  if (ENABLE_USING_LINENOISE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_LINENOISE)
  endif()
endif()

if(DEVELOPER_MODE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DDEVELOPER_MODE)
endif()

if(PORTABLE_MODE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DPORTABLE_MODE)
endif()

if(ALLOW_COMMAND_LINE_RESOURCE_LOAD)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DALLOW_COMMAND_LINE_RESOURCE_LOAD)
endif()

if(USE_DEFAULT_FONTS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSE_DEFAULT_FONTS)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DTARGET_NAME=\"${HATCH_TARGET_NAME}\")

set(CONSOLE_BASE_PATH "" CACHE STRING "Optional base path (must end with '/')")
if(NOT CONSOLE_BASE_PATH STREQUAL "")
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DCONSOLE_BASE_PATH="${CONSOLE_BASE_PATH}")
endif()

find_file(REPO_DIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

if (REPO_DIR)
  execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  target_compile_definitions(${PROJECT_NAME} PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
endif()

# Add needed directories
include_directories(
  include
  source
  ${SDL2_INCLUDE_DIRS}
)

# Add libraries
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})

if(PNG_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_LIBPNG)
  target_link_libraries(${PROJECT_NAME} ${PNG_LIBRARIES})
endif()

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  if(assimp_FOUND)
    include_directories(${ASSIMP_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_ASSIMP)
    target_link_libraries(${PROJECT_NAME} ${ASSIMP_LIBRARIES})
  endif()
endif()

if(USING_OPENGL)
  target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()
