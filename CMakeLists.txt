cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(HatchGameEngine VERSION 1.1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=nocona")

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

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(WINDOWS_USE_RESOURCE_FILE "Use resource file (Windows)" ON)
endif()

option(USE_OPEN_ASSET_IMPORT_LIBRARY "Use Open Asset Import Library" ON)
option(USE_FREETYPE_LIBRARY "Use FreeType" OFF)

# Renderers
option(USING_OPENGL "Use OpenGL" ON)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(USING_DIRECT3D "Use Direct3D" OFF)
endif()

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  add_definitions(-DUSING_ASSIMP)
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(MAKEHEADERS_BINARY makeheaders.exe)
else()
  set(MAKEHEADERS_BINARY makeheaders)
endif()

add_custom_target(makeheaders ALL
  COMMAND ${CMAKE_SOURCE_DIR}/tools/${MAKEHEADERS_BINARY} source/
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  BYPRODUCTS "${CMAKE_SOURCE_DIR}/include/makeheaders.bin" "${CMAKE_SOURCE_DIR}/include/Engine"
  COMMENT "Generating header files")

add_executable(${PROJECT_NAME} ${HATCH_SOURCES})
add_dependencies(${PROJECT_NAME} makeheaders)

set(OUT_EXEC_NAME ${PROJECT_NAME})

# Change executable name
if(EXECUTABLE_NAME)
  set(OUT_EXEC_NAME ${EXECUTABLE_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${OUT_EXEC_NAME}-${CMAKE_BUILD_TYPE}")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(WIN_RES_FILE meta/win/icon.rc)

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
    find_package(assimp REQUIRED)
    set(ASSIMP_INCLUDE_DIRS assimp_INCLUDE_DIRS)
    set(ASSIMP_LIBRARIES assimp)
  endif()
  if(USING_OPENGL)
    set(OPENGL_LIBRARIES "-lGL -lGLEW")
  endif()
else()
  set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/SDL2/")
  if(USE_OPEN_ASSET_IMPORT_LIBRARY)
    set(ASSIMP_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/assimp/")
    set(ASSIMP_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libassimp.a;${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libzlibstatic.a")
  endif()
  if(USING_OPENGL)
    set(OPENGL_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libglew32.a;-lgdi32;-lopengl32")
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
  target_link_libraries(${PROJECT_NAME}
    "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libSDL2.a"
    -static -mwindows -lmingw32 -lm -ldinput8 -ldxguid -ldxerr8
    -luser32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32
    -lversion -luuid -lhid -lsetupapi -lws2_32)
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
