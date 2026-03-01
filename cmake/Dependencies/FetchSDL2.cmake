include_guard(GLOBAL)
include(cmake/CPM.cmake)

# SDL2 static build
CPMAddPackage(
  NAME SDL2
  URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.32.10.zip
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "EXCLUDE_FROM_ALL ON"
    "SDL2_DISABLE_SDL2MAIN ON"
    "SDL_TEST OFF"
)
set(SDL2_LIBRARIES SDL2::SDL2-static)
