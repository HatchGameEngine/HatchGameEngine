add_executable(${PROJECT_NAME} ${HATCH_SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DSWITCH -DCONSOLE_FILESYSTEM)

find_package(SDL2 REQUIRED)

find_package(PNG)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  # Force find zlib dependency
  find_package(ZLIB REQUIRED)
  include(cmake/Dependencies/Fetchassimp.cmake)
endif()

if(USING_OPENGL)
  set(OPENGL_LIBRARIES "GLESv2")
endif()

set(ICON_FILE "meta/nx/icon.jpg" CACHE FILEPATH "Optional path to a jpg icon")
set(ROMFS_PATH "" CACHE PATH "Optional path to a folder containing game data")
if (NOT ROMFS_PATH STREQUAL "")
  message(STATUS "SWITCH: Building with romfs: ${ROMFS_PATH}")
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DSWITCH_ROMFS)
  nx_create_nro(${PROJECT_NAME} ICON ${ICON_FILE} ROMFS ${ROMFS_PATH})
else()
  message(STATUS "SWITCH: Building without romfs")
  nx_create_nro(${PROJECT_NAME} ICON ${ICON_FILE})
endif()
