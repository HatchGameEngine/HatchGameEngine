add_library(${PROJECT_NAME} SHARED ${HATCH_SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DANDROID -DCONSOLE_FILESYSTEM -DCONSOLE_BASE_PATH="./")

# Force shared library for SDL2
set(SDL2_AS_SHARED_LIBRARY TRUE)
include(cmake/Dependencies/FetchSDL2.cmake)

# SDL2 requires some java code to function and bootstrap our code.
# This code may change with future updates, and needs to be in sync with its C code.
# This will clear the previous files and copy the new ones from the fetched SDL2 version automatically.
set(SDL_ACTIVITY_DIR app/src/main/java/org/libsdl/app/)
set(HATCH_SRC_SDL_JAVA_DIR ${CMAKE_SOURCE_DIR}/Android-Project/${SDL_ACTIVITY_DIR})

file(REMOVE_RECURSE ${HATCH_SRC_SDL_JAVA_DIR})
file(MAKE_DIRECTORY ${HATCH_SRC_SDL_JAVA_DIR})
file(COPY ${SDL2_SOURCE_DIR}/android-project/${SDL_ACTIVITY_DIR} DESTINATION ${HATCH_SRC_SDL_JAVA_DIR})

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  # Android already has a zlib system library, just act like it's already available
  set(ZLIB_FOUND TRUE)
  include(cmake/Dependencies/Fetchassimp.cmake)
endif()

if(USING_OPENGL)
  set(OPENGL_LIBRARIES "-lEGL -lGLESv2")
endif()

target_link_libraries(${PROJECT_NAME} android log)

# Rename the Hatch shared library unconditionally so that our SDL/HatchActivity can find and load it
set(OUT_EXEC_NAME "HatchGameEngine")
# Remove debug postfix "d" for Hatch
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "")

# TODO: Add DEVELOPER_MODE version that loads from a folder instead? Need to change CONSOLE_BASE_PATH above?
