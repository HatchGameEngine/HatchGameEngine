add_definitions(-DSWITCH -DCONSOLE_FILESYSTEM)

find_package(SDL2 REQUIRED)

find_package(PNG)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  set(assimp_FOUND TRUE)
  # No need to duplicate headers, use the ones for Windows
  set(ASSIMP_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/meta/win/include/")
  set(ASSIMP_LIBRARIES "${CMAKE_SOURCE_DIR}/meta/nx/lib/libassimp.a" "-lz -lminizip")
endif()

if(USING_OPENGL)
  set(OPENGL_LIBRARIES "GLESv2")
endif()

set(ICON_FILE "meta/nx/icon.jpg" CACHE FILEPATH "Optional path to a jpg icon")
set(ROMFS_PATH "" CACHE PATH "Optional path to a folder containing game data")
if (NOT ROMFS_PATH STREQUAL "")
  message(STATUS "SWITCH: Building with romfs: ${ROMFS_PATH}")
  add_definitions(-DSWITCH_ROMFS)
  nx_create_nro(${PROJECT_NAME} ICON ${ICON_FILE} ROMFS ${ROMFS_PATH})
else()
  message(STATUS "SWITCH: Building without romfs")
  nx_create_nro(${PROJECT_NAME} ICON ${ICON_FILE})
endif()
