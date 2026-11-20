add_executable(${PROJECT_NAME} ${HATCH_SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DLINUX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Winline -Wempty-body -Winit-self -Wshadow=local -Wno-missing-field-initializers -Wno-unused-parameter -Wno-template-id-cdtor -Wno-c++20-compat")

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
