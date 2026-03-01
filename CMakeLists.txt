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
option(USING_HSL_VM "Build with HSL VM" ON)
option(USING_HSL_COMPILER "Build with HSL compiler" ON)
option(USING_HSL_STANDARD_LIBRARY "Build with HSL standard library" ON)
option(ENABLE_SCRIPT_COMPILING "Enable script compiling" ON)
option(ENABLE_USING_VM_FUNCPTRS "Enable function-pointer based VM" ON)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  option(ENABLE_VM_DEBUGGING "Enable VM debugging" ON)
  option(DEVELOPER_MODE "Developer mode" ON)
else()
  option(ENABLE_VM_DEBUGGING "Enable VM debugging" OFF)
  option(DEVELOPER_MODE "Developer mode" OFF)
endif()

option(BUILD_HSL_PROGRAM "Build hsl program" OFF)
option(BUILD_HSLC_PROGRAM "Build hslc program" OFF)
option(BUILD_LIBHSL "Build libhsl" OFF)

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

# Find platform cmake file
find_file(PLATFORM_FILE NAMES "${CMAKE_SYSTEM_NAME}.cmake" PATHS "${CMAKE_SOURCE_DIR}/cmake/Platforms" NO_DEFAULT_PATH)
if(NOT PLATFORM_FILE)
  message(FATAL_ERROR "Could not find cmake file for target: '${CMAKE_SYSTEM_NAME}'")
endif()
include(${PLATFORM_FILE})

set(OUT_EXEC_NAME ${PROJECT_NAME}-${CMAKE_BUILD_TYPE})

# Change executable name
if(EXECUTABLE_NAME)
  set(OUT_EXEC_NAME ${EXECUTABLE_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${OUT_EXEC_NAME}")

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DMEMORY_TRACKING)
endif()

if(USING_OPENGL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_OPENGL)
endif()

if(USING_HSL_STANDARD_LIBRARY)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DHSL_STDLIB)
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

target_compile_definitions(${PROJECT_NAME} PRIVATE USE_MEMORY_CACHE USE_ZLIB USE_CLOCK HSL_VM HSL_COMPILER)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DTARGET_NAME=\"${HATCH_TARGET_NAME}\")

set(CONSOLE_BASE_PATH "" CACHE STRING "Optional base path (must end with '/')")
if(NOT CONSOLE_BASE_PATH STREQUAL "")
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DCONSOLE_BASE_PATH="${CONSOLE_BASE_PATH}")
endif()

find_file(REPO_DIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)

if(REPO_DIR)
  execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  target_compile_definitions(${PROJECT_NAME} PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
endif()

# Add needed directories
target_include_directories(${PROJECT_NAME} PUBLIC
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

set(HSL_MAIN_SOURCES
  source/Engine/Bytecode/Bytecode.cpp
  source/Engine/Bytecode/BytecodeDebugger.cpp
  source/Engine/Bytecode/ScriptManager.cpp
  source/Engine/Bytecode/ScriptREPL.cpp
  source/Engine/Bytecode/TypeImpl/ArrayImpl.cpp
  source/Engine/Bytecode/TypeImpl/FunctionImpl.cpp
  source/Engine/Bytecode/TypeImpl/InstanceImpl.cpp
  source/Engine/Bytecode/TypeImpl/MapImpl.cpp
  source/Engine/Bytecode/TypeImpl/StreamImpl.cpp
  source/Engine/Bytecode/TypeImpl/StringImpl.cpp
  source/Engine/Bytecode/TypeImpl/TypeImpl.cpp
  source/Engine/Bytecode/Types.cpp
  source/Engine/Bytecode/Value.cpp
  source/Engine/Bytecode/ValuePrinter.cpp
  source/Engine/Diagnostics/Log.cpp
  source/Engine/Diagnostics/Memory.cpp
  source/Engine/Error.cpp
  source/Engine/Filesystem/Directory.cpp
  source/Engine/Filesystem/File.cpp
  source/Engine/Filesystem/Path.cpp
  source/Engine/Hashing/CombinedHash.cpp
  source/Engine/Hashing/CRC32.cpp
  source/Engine/Hashing/FNV1A.cpp
  source/Engine/Hashing/MD5.cpp
  source/Engine/Hashing/Murmur.cpp
  source/Engine/IO/FileStream.cpp
  source/Engine/IO/MemoryStream.cpp
  source/Engine/IO/StandardIOStream.cpp
  source/Engine/IO/Stream.cpp
  source/Engine/IO/TerminalInput.cpp
  source/Engine/IO/TextStream.cpp
  source/Engine/Utilities/PrintBuffer.cpp
  source/Engine/Utilities/StringUtils.cpp
)
set(HSL_VM_SOURCES
  source/Engine/Bytecode/GarbageCollector.cpp
  source/Engine/Bytecode/VMThread.cpp
  source/Engine/Bytecode/VMThreadDebugger.cpp
)
set(HSL_COMPILER_SOURCES
  source/Engine/Bytecode/Compiler.cpp
  source/Engine/Bytecode/SourceFileMap.cpp
)

# HSL standalone program
if(BUILD_HSL_PROGRAM)
  add_executable(hsl ${HSL_MAIN_SOURCES} source/Engine/Main.cpp source/Engine/Bytecode/StandaloneMain.cpp)

  option(ENABLE_REPL "Enable REPL" ON)
  option(ENABLE_LOG_FILE "Enable logging output to a file" ON)
  if(USING_HSL_STANDARD_LIBRARY)
    option(USING_CLOCK "Build with clock" ON)
  else()
    option(USING_CLOCK "Build with clock" OFF)
  endif()

  target_compile_definitions(hsl PRIVATE -DHSL_STANDALONE -DNO_SCRIPT_COMPILING -DTARGET_NAME=\"hsl\")
  if(DEFINED GIT_COMMIT_HASH)
    target_compile_definitions(hsl PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
  endif()
  target_include_directories(hsl PUBLIC
    include
    source
  )
  if(ENABLE_LOG_FILE)
    target_compile_definitions(hsl PRIVATE -DUSE_LOG_FILE)
  endif()
  if(USING_CLOCK)
    target_compile_definitions(hsl PRIVATE -DUSE_CLOCK)
    target_sources(hsl PUBLIC source/Engine/Diagnostics/Clock.cpp)
  endif()
  if(USING_HSL_VM)
    target_compile_definitions(hsl PRIVATE -DHSL_VM -DHSL_STANDALONE_RUNNER)
    target_sources(hsl PUBLIC ${HSL_VM_SOURCES})
  endif()
  if(USING_HSL_COMPILER)
    target_compile_definitions(hsl PRIVATE -DHSL_COMPILER)
    target_sources(hsl PUBLIC ${HSL_COMPILER_SOURCES})
  endif()
  if(USING_HSL_VM AND ENABLE_VM_DEBUGGING)
    target_compile_definitions(hsl PRIVATE -DVM_DEBUG)
  endif()
  if(USING_HSL_VM AND ENABLE_USING_VM_FUNCPTRS)
    target_compile_definitions(hsl PRIVATE -DUSING_VM_FUNCPTRS)
  endif()
  if(ENABLE_USING_LINENOISE)
    target_sources(hsl PUBLIC
      source/Libraries/linenoise-ng/linenoise.cpp
      source/Libraries/linenoise-ng/wcwidth.cpp
    )
    target_compile_definitions(hsl PRIVATE -DUSING_LINENOISE)
  endif()
  if(ENABLE_REPL)
    target_compile_definitions(hsl PRIVATE -DHSL_STANDALONE_REPL)
  endif()
endif()

# HSL compiler
if(BUILD_HSLC_PROGRAM)
  add_executable(hslc ${HSL_MAIN_SOURCES} ${HSL_COMPILER_SOURCES} source/Engine/Main.cpp source/Engine/Bytecode/StandaloneMain.cpp)

  target_compile_definitions(hslc PRIVATE -DHSL_STANDALONE -DHSL_COMPILER -DNO_SCRIPT_COMPILING -DTARGET_NAME=\"hslc\")
  if(DEFINED GIT_COMMIT_HASH)
    target_compile_definitions(hslc PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
  endif()
  target_include_directories(hslc PUBLIC
    include
    source
  )
endif()

# HSL library
if(BUILD_LIBHSL)
  add_library(libhsl SHARED ${HSL_MAIN_SOURCES} source/Engine/Bytecode/API.cpp source/Engine/Bytecode/StandaloneMain.cpp)
  set_target_properties(libhsl PROPERTIES OUTPUT_NAME hsl)
  set_target_properties(libhsl PROPERTIES VERSION 0.0.1 SOVERSION 0)

  target_compile_definitions(libhsl PRIVATE -DHSL_LIBRARY -DHSL_STANDALONE -DNO_SCRIPT_COMPILING -DTARGET_NAME=\"libhsl\")
  if(DEFINED GIT_COMMIT_HASH)
    target_compile_definitions(libhsl PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
  endif()
  target_include_directories(libhsl PUBLIC
    include
    source
  )
  target_compile_definitions(libhsl PRIVATE)
  if(USING_HSL_VM)
    target_compile_definitions(libhsl PRIVATE -DHSL_VM -DHSL_STANDALONE_RUNNER)
    target_sources(libhsl PUBLIC ${HSL_VM_SOURCES})
  endif()
  if(USING_HSL_COMPILER)
    target_compile_definitions(libhsl PRIVATE -DHSL_COMPILER)
    target_sources(libhsl PUBLIC ${HSL_COMPILER_SOURCES})
  endif()
  if(USING_HSL_VM AND ENABLE_VM_DEBUGGING)
    target_compile_definitions(libhsl PRIVATE -DVM_DEBUG)
  endif()
  if(USING_HSL_VM AND ENABLE_USING_VM_FUNCPTRS)
    target_compile_definitions(libhsl PRIVATE -DUSING_VM_FUNCPTRS)
  endif()
endif()
