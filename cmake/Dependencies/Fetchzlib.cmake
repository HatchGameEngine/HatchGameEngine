include_guard(GLOBAL)
include(cmake/CPM.cmake)

# zlib-ng static build
CPMAddPackage(
  NAME zlib
  URL https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.3.3.zip
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "EXCLUDE_FROM_ALL ON"
    "BUILD_TESTING OFF"
    "ZLIB_COMPAT ON"
)