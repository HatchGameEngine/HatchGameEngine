cmake_minimum_required(VERSION 3.13.0)

# set the project name and version
project(HatchGameEngine VERSION 1.4.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_CXX_FLAGS "/wd4244 /wd4267 /wd4305 /wd4717 /EHsc")
else()
  set(HATCH_OPT_FLAGS "-Ofast")
  if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    set(HATCH_OPT_FLAGS "${HATCH_OPT_FLAGS} -march=nocona")
  endif()
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${HATCH_OPT_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${HATCH_OPT_FLAGS}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${HATCH_OPT_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${HATCH_OPT_FLAGS}")
endif()

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

add_executable(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES} ${LIBRARY_SOURCES})
target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SDL USE_MEMORY_CACHE USE_ZLIB USE_CLOCK HSL_COMPILER)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DMEMORY_TRACKING)
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DTARGET_NAME=\"${HATCH_TARGET_NAME}\"
   -DNO_LIBAV
  )
  if(NINTENDO_SWITCH)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DSWITCH -DCONSOLE_FILESYSTEM)
  else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DLINUX)
  endif()

  set(CONSOLE_BASE_PATH "" CACHE STRING "Optional base path (must end with '/')")
  if(NOT CONSOLE_BASE_PATH STREQUAL "")
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DCONSOLE_BASE_PATH="${CONSOLE_BASE_PATH}")
  endif()
else()
  # Cross compiling a Windows executable
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DWIN32 -DTARGET_NAME=\"${HATCH_TARGET_NAME}\"
   -DNO_LIBAV
   -DGLEW_STATIC
   -D_WINDOWS
  )
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
endif()

option(PORTABLE_MODE "Portable mode" OFF)
option(ALLOW_COMMAND_LINE_RESOURCE_LOAD "Allow loading resource files from command line" ON)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(WINDOWS_USE_RESOURCE_FILE "Use resource file (Windows)" ON)
  if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" ON)
  else()
    option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" OFF)
  endif()
endif()

option(USE_OPEN_ASSET_IMPORT_LIBRARY "Use Open Asset Import Library" ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(USE_OPEN_ASSET_IMPORT_LIBRARY OFF)
endif()

option(USE_DEFAULT_FONTS "Build with default fonts" ON)

# Renderers
option(USING_OPENGL "Use OpenGL" ON)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(USING_DIRECT3D "Use Direct3D" OFF)
endif()

if(USING_OPENGL)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_OPENGL)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND USING_DIRECT3D)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_DIRECT3D)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC -DHSL_VM)

if(USING_HSL_STANDARD_LIBRARY)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DHSL_STDLIB)
endif()

if(NOT ENABLE_SCRIPT_COMPILING)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DNO_SCRIPT_COMPILING)
endif()

if(USING_HSL_VM AND ENABLE_USING_VM_FUNCPTRS)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_VM_FUNCPTRS)
endif()

if(ENABLE_VM_DEBUGGING)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DVM_DEBUG)

  if(ENABLE_USING_LINENOISE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_LINENOISE)
    target_sources(${PROJECT_NAME} PUBLIC
      source/Libraries/linenoise-ng/linenoise.cpp
      source/Libraries/linenoise-ng/wcwidth.cpp
    )
  endif()
endif()

if(DEVELOPER_MODE)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DDEVELOPER_MODE)
endif()

if(PORTABLE_MODE)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DPORTABLE_MODE)
endif()

if(ALLOW_COMMAND_LINE_RESOURCE_LOAD)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DALLOW_COMMAND_LINE_RESOURCE_LOAD)
endif()

if(USE_DEFAULT_FONTS)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSE_DEFAULT_FONTS)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME)

find_file(REPO_DIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)

if(REPO_DIR)
  execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  target_compile_definitions(${PROJECT_NAME} PUBLIC "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
endif()

set(OUT_EXEC_NAME ${PROJECT_NAME}-${CMAKE_BUILD_TYPE})

# Change executable name
if(EXECUTABLE_NAME)
  set(OUT_EXEC_NAME ${EXECUTABLE_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${OUT_EXEC_NAME}")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(WIN_RES_FILE meta/win/HatchGameEngine.rc)

  if(RESOURCE_FILE)
    set(WIN_RES_FILE ${RESOURCE_FILE})
  endif()
endif()

# Find the includes
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  find_package(SDL2 REQUIRED)
  find_package(PNG)
  if(PNG_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_LIBPNG)
  endif()
  if(USE_OPEN_ASSET_IMPORT_LIBRARY)
    if(NINTENDO_SWITCH)
      target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_ASSIMP)
      # No need to duplicate headers, use the ones for Windows
      set(ASSIMP_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/")
      set(ASSIMP_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/nx/lib/libassimp.a" "-lz -lminizip")
    else()
      find_package(assimp)
      if(assimp_FOUND)
        target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_ASSIMP)
        set(ASSIMP_INCLUDE_DIRS "${assimp_INCLUDE_DIRS}")
        set(ASSIMP_LIBRARIES "${assimp_LIBRARIES}")
      endif()
    endif()
  endif()
  if(USING_OPENGL)
    if(NINTENDO_SWITCH)
      set(OPENGL_LIBRARIES "GLESv2")
    else()
      set(OPENGL_LIBRARIES "-lGL -lGLEW")
    endif()
  endif()
else()
  set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/SDL2/")
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(USING_OPENGL)
      set(OPENGL_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/msvc/x64/glew32s.lib")
    endif()
  else()
    if(USE_OPEN_ASSET_IMPORT_LIBRARY)
      target_compile_definitions(${PROJECT_NAME} PUBLIC -DUSING_ASSIMP)
      set(ASSIMP_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/assimp/")
      set(ASSIMP_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libassimp.a;${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libzlibstatic.a")
    endif()
    if(USING_OPENGL)
      set(OPENGL_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libglew32.a;-lgdi32;-lopengl32")
    endif()
  endif()
endif()

# Add needed directories
target_include_directories(${PROJECT_NAME} PUBLIC
  include
  source
  ${SDL2_INCLUDE_DIRS}
  ${ASSIMP_INCLUDE_DIRS}
)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  target_include_directories(${PROJECT_NAME} PUBLIC ${ASSIMP_INCLUDE_DIRS})
endif()

# Include libraries
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # Add all of the Windows include files
  target_include_directories(${PROJECT_NAME} PUBLIC
    meta/win/include
  )
  if(WINDOWS_USE_RESOURCE_FILE)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/${WIN_RES_FILE})
  endif()
endif()

# Add libraries
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_link_libraries(${PROJECT_NAME} "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/msvc/x64/SDL2.lib"
    Ws2_32.lib opengl32.lib winmm.lib imm32.lib version.lib setupapi.lib)
  else()
    target_link_libraries(${PROJECT_NAME}
      "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libSDL2.a"
      -static -lmingw32 -lm -ldinput8 -ldxguid -ldxerr8
      -luser32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32
      -lversion -luuid -lhid -lsetupapi -lws2_32)

    if(NOT WINDOWS_COMPILE_AS_CONSOLE_APP)
      target_link_libraries(${PROJECT_NAME} -mwindows)
    endif()
  endif()
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DSDL_MAIN_HANDLED)
else()
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()

if(PNG_FOUND)
  target_link_libraries(${PROJECT_NAME} ${PNG_LIBRARIES})
endif()
if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  target_link_libraries(${PROJECT_NAME} ${ASSIMP_LIBRARIES})
endif()
if(USING_OPENGL)
  target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()

if(NINTENDO_SWITCH)
  set(ICON_FILE "meta/nx/icon.jpg" CACHE FILEPATH "Optional path to a jpg icon")
  set(ROMFS_PATH "" CACHE PATH "Optional path to a folder containing game data")
  if(NOT ROMFS_PATH STREQUAL "")
    message(STATUS "SWITCH: Building with romfs: ${ROMFS_PATH}")
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DSWITCH_ROMFS)
    nx_create_nro(${PROJECT_NAME} ICON ${ICON_FILE} ROMFS ${ROMFS_PATH})
  else()
    message(STATUS "SWITCH: Building without romfs")
    nx_create_nro(${PROJECT_NAME} ICON ${ICON_FILE})
  endif()
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

  target_compile_definitions(hsl PUBLIC -DHSL_STANDALONE -DNO_SCRIPT_COMPILING -DTARGET_NAME=\"hsl\")
  if(DEFINED GIT_COMMIT_HASH)
    target_compile_definitions(hsl PUBLIC "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
  endif()
  target_include_directories(hsl PUBLIC
    include
    source
  )
  if(ENABLE_LOG_FILE)
    target_compile_definitions(hsl PUBLIC -DUSE_LOG_FILE)
  endif()
  if(USING_CLOCK)
    target_compile_definitions(hsl PUBLIC -DUSE_CLOCK)
    target_sources(hsl PUBLIC source/Engine/Diagnostics/Clock.cpp)
  endif()
  if(USING_HSL_VM)
    target_compile_definitions(hsl PUBLIC -DHSL_VM -DHSL_STANDALONE_RUNNER)
    target_sources(hsl PUBLIC ${HSL_VM_SOURCES})
  endif()
  if(USING_HSL_COMPILER)
    target_compile_definitions(hsl PUBLIC -DHSL_COMPILER)
    target_sources(hsl PUBLIC ${HSL_COMPILER_SOURCES})
  endif()
  if(USING_HSL_VM AND ENABLE_VM_DEBUGGING)
    target_compile_definitions(hsl PUBLIC -DVM_DEBUG)
  endif()
  if(USING_HSL_VM AND ENABLE_USING_VM_FUNCPTRS)
    target_compile_definitions(hsl PUBLIC -DUSING_VM_FUNCPTRS)
  endif()
  if(ENABLE_USING_LINENOISE)
    target_sources(hsl PUBLIC
      source/Libraries/linenoise-ng/linenoise.cpp
      source/Libraries/linenoise-ng/wcwidth.cpp
    )
    target_compile_definitions(hsl PUBLIC -DUSING_LINENOISE)
  endif()
  if(ENABLE_REPL)
    target_compile_definitions(hsl PUBLIC -DHSL_STANDALONE_REPL)
  endif()
endif()

# HSL compiler
if(BUILD_HSLC_PROGRAM)
  add_executable(hslc ${HSL_MAIN_SOURCES} ${HSL_COMPILER_SOURCES} source/Engine/Main.cpp source/Engine/Bytecode/StandaloneMain.cpp)

  target_compile_definitions(hslc PUBLIC -DHSL_STANDALONE -DHSL_COMPILER -DNO_SCRIPT_COMPILING -DTARGET_NAME=\"hslc\")
  if(DEFINED GIT_COMMIT_HASH)
    target_compile_definitions(hslc PUBLIC "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
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

  target_compile_definitions(libhsl PUBLIC -DHSL_LIBRARY -DHSL_STANDALONE -DNO_SCRIPT_COMPILING -DTARGET_NAME=\"libhsl\")
  if(DEFINED GIT_COMMIT_HASH)
    target_compile_definitions(libhsl PUBLIC "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
  endif()
  target_include_directories(libhsl PUBLIC
    include
    source
  )
  target_compile_definitions(libhsl PUBLIC)
  if(USING_HSL_VM)
    target_compile_definitions(libhsl PUBLIC -DHSL_VM -DHSL_STANDALONE_RUNNER)
    target_sources(libhsl PUBLIC ${HSL_VM_SOURCES})
  endif()
  if(USING_HSL_COMPILER)
    target_compile_definitions(libhsl PUBLIC -DHSL_COMPILER)
    target_sources(libhsl PUBLIC ${HSL_COMPILER_SOURCES})
  endif()
  if(USING_HSL_VM AND ENABLE_VM_DEBUGGING)
    target_compile_definitions(libhsl PUBLIC -DVM_DEBUG)
  endif()
  if(USING_HSL_VM AND ENABLE_USING_VM_FUNCPTRS)
    target_compile_definitions(libhsl PUBLIC -DUSING_VM_FUNCPTRS)
  endif()
endif()