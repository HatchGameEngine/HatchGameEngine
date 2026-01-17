add_definitions(-DLINUX)

find_package(SDL2 REQUIRED)

find_package(PNG)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  find_package(assimp)
  if(assimp_FOUND)
    set(ASSIMP_INCLUDE_DIRS "${assimp_INCLUDE_DIRS}")
    set(ASSIMP_LIBRARIES "${assimp_LIBRARIES}")
  endif()
endif()

if(USING_OPENGL)
  set(OPENGL_LIBRARIES "-lGL -lGLEW")
endif()
