cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(HatchGameEngine VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set up CMake modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

file(GLOB_RECURSE C_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/*.c")
file(GLOB_RECURSE CPP_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "source/*.cpp")

set(HATCH_SOURCES ${C_SOURCES} ${CPP_SOURCES})

set(HATCH_TARGET_NAME ${PROJECT_NAME})
IF(TARGET_NAME)
  set(HATCH_TARGET_NAME ${TARGET_NAME})
endif()

IF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  add_definitions(-DLINUX -DTARGET_NAME=\"${HATCH_TARGET_NAME}\"
   -DUSING_FREETYPE
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

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  option(WINDOWS_USE_RESOURCE_FILE "Use resource file (Windows)" ON)
endif()

if(NOT ENABLE_SCRIPT_COMPILING)
  add_definitions(-DNO_SCRIPT_COMPILING)
endif()

add_custom_target(makeheaders ALL
  COMMAND ${CMAKE_SOURCE_DIR}/tools/lmakeheaders source/
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  BYPRODUCTS "${CMAKE_SOURCE_DIR}/makeheaders.bin" "${CMAKE_SOURCE_DIR}/include/Engine"
  COMMENT "Generating header files")

add_executable(${PROJECT_NAME} ${HATCH_SOURCES})
add_dependencies(${PROJECT_NAME} makeheaders)

set(OUT_EXEC_NAME ${PROJECT_NAME})

# Change executable name
IF(EXECUTABLE_NAME)
  set(OUT_EXEC_NAME ${EXECUTABLE_NAME})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${OUT_EXEC_NAME}-${CMAKE_BUILD_TYPE}")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(WIN_RES_FILE meta/win/icon.rc)

  if(RESOURCE_FILE)
    set(WIN_RES_FILE ${RESOURCE_FILE})
  endif()
endif()

# Find the includes
IF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  find_package(SDL2 REQUIRED)
  find_package(Freetype REQUIRED)
else()
  set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/meta/win/include/SDL2/")
endif()

# Add needed directories
include_directories(
  include
  source
  ${SDL2_INCLUDE_DIRS}
)

# Include libraries
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # Add all of the Windows include files
  include_directories(
    meta/win/include
  )
  if(WINDOWS_USE_RESOURCE_FILE)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/${WIN_RES_FILE})
  endif()
else()
  include_directories(
    ${FREETYPE_INCLUDE_DIRS}
  )
endif()

# Add libraries
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_link_libraries(${PROJECT_NAME}
    "${CMAKE_CURRENT_LIST_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libSDL2.a"
    -static -mwindows -lmingw32 -lm -ldinput8 -ldxguid -ldxerr8
    -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32
    -lversion -luuid -lhid -lsetupapi -lws2_32)
  add_definitions(-DSDL_MAIN_HANDLED)
else()
  target_link_libraries(${PROJECT_NAME}
    ${FREETYPE_LIBRARIES}
  )
endif()
