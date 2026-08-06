include_guard(GLOBAL)
include(cmake/CPM.cmake)

# ZLIB is a dependency, but it's also a very common library available on many systems.
# To simplify things, let's avoid fetching it if the platform already used find_package(ZLIB).
if(NOT ZLIB_FOUND)
  include(cmake/Dependencies/Fetchzlib.cmake)
endif()

# assimp static build
CPMAddPackage(
  NAME assimp
  URL https://github.com/assimp/assimp/archive/refs/tags/v6.0.4.zip
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "EXCLUDE_FROM_ALL ON"
    "ZLIB_USE_STATIC_LIBS ON"
    "ASSIMP_BUILD_ZLIB OFF"
    "ASSIMP_BUILD_MINIZIP OFF"
    "ASSIMP_INSTALL OFF"
    "ASSIMP_BUILD_TESTS OFF"
    "ASSIMP_NO_EXPORT ON"
    "ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF"
    "ASSIMP_BUILD_FBX_IMPORTER ON"
    "ASSIMP_BUILD_OBJ_IMPORTER ON"
    "ASSIMP_BUILD_GLTF_IMPORTER ON"
    "ASSIMP_BUILD_COLLADA_IMPORTER ON"
)
set(assimp_FOUND TRUE)

if(ZLIB_FOUND)
  set(ASSIMP_LIBRARIES assimp z)
else()
  set(ASSIMP_LIBRARIES assimp zlibstatic)
endif()

target_include_directories(assimp PUBLIC ${zlib_BINARY_DIR})
