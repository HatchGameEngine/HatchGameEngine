include_guard(GLOBAL)
include(cmake/CPM.cmake)

# glew static build
CPMAddPackage(
  NAME glew
  URL https://github.com/Perlmint/glew-cmake/archive/refs/tags/glew-cmake-2.3.1.zip
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "EXCLUDE_FROM_ALL ON"
)
set(OPENGL_LIBRARIES libglew_static)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DGLEW_STATIC)
target_include_directories(${PROJECT_NAME} PRIVATE ${glew_SOURCE_DIR}/include)
