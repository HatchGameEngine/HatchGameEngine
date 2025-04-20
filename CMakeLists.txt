cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(HatchGameEngine VERSION 1.3.0)

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

file(GLOB_RECURSE C_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/*.c")
file(GLOB_RECURSE CPP_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/*.cpp")

set(HATCH_SOURCES ${C_SOURCES} ${CPP_SOURCES})

set(HATCH_TARGET_NAME ${PROJECT_NAME})
if(TARGET_NAME)
  set(HATCH_TARGET_NAME ${TARGET_NAME})
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  add_definitions(-DLINUX -DTARGET_NAME=\"${HATCH_TARGET_NAME}\"
   -DNO_LIBAV
  )
else()
  # Cross compiling a Windows executable
  add_definitions(-DWIN32 -DTARGET_NAME=\"${HATCH_TARGET_NAME}\"
   -DNO_LIBAV
   -DGLEW_STATIC
   -D_WINDOWS
  )
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(WINDOWS_USE_RESOURCE_FILE "Use resource file (Windows)" ON)
  if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" ON)
  else()
    option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" OFF)
  endif()
endif()

option(USE_OPEN_ASSET_IMPORT_LIBRARY "Use Open Asset Import Library" ON)
option(USE_FREETYPE_LIBRARY "Use FreeType" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(USE_OPEN_ASSET_IMPORT_LIBRARY OFF)
endif()

# Renderers
option(USING_OPENGL "Use OpenGL" ON)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(USING_DIRECT3D "Use Direct3D" OFF)
endif()

if(USE_FREETYPE_LIBRARY)
  add_definitions(-DUSING_FREETYPE)
endif()

if(USING_OPENGL)
  add_definitions(-DUSING_OPENGL)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND USING_DIRECT3D)
  add_definitions(-DUSING_DIRECT3D)
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

add_definitions(-DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME )

find_file(REPO_DIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)

if (REPO_DIR)
  execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  add_definitions("-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
endif()

add_executable(${PROJECT_NAME} ${HATCH_SOURCES})

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
  if(USE_FREETYPE_LIBRARY)
    find_package(Freetype REQUIRED)
  endif()
  find_package(PNG)
  if(PNG_FOUND)
    add_definitions(-DUSING_LIBPNG)
  endif()
  if(USE_OPEN_ASSET_IMPORT_LIBRARY)
    find_package(assimp)
    if(assimp_FOUND)
      add_definitions(-DUSING_ASSIMP)
      set(ASSIMP_INCLUDE_DIRS "${assimp_INCLUDE_DIRS}")
      set(ASSIMP_LIBRARIES "${assimp_LIBRARIES}")
    endif()
  endif()
  if(USING_OPENGL)
    set(OPENGL_LIBRARIES "-lGL -lGLEW")
  endif()
else()
  set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/SDL2/")
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(USING_OPENGL)
      set(OPENGL_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/msvc/x64/glew32s.lib")
    endif()
  else()
    if(USE_OPEN_ASSET_IMPORT_LIBRARY)
      add_definitions(-DUSING_ASSIMP)
      set(ASSIMP_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/assimp/")
      set(ASSIMP_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libassimp.a;${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libzlibstatic.a")
    endif()
    if(USING_OPENGL)
      set(OPENGL_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libglew32.a;-lgdi32;-lopengl32")
    endif()
  endif()
endif()

# Add needed directories
include_directories(
  include
  source
  ${SDL2_INCLUDE_DIRS}
  ${ASSIMP_INCLUDE_DIRS}
)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  include_directories(${ASSIMP_INCLUDE_DIRS})
endif()

# Include libraries
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # Add all of the Windows include files
  include_directories(
    meta/win/include
  )
  if(WINDOWS_USE_RESOURCE_FILE)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/${WIN_RES_FILE})
  endif()
else()
  if(USE_FREETYPE_LIBRARY)
    include_directories(
      ${FREETYPE_INCLUDE_DIRS}
    )
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
  add_definitions(-DSDL_MAIN_HANDLED)
else()
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()

if(USE_FREETYPE_LIBRARY)
  target_link_libraries(${PROJECT_NAME} ${FREETYPE_LIBRARIES})
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
