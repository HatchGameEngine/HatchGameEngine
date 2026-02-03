# CMake platform files

This folder contains platform specific directives for CMake build.

## Porting to a new platform

New platforms must follow the naming convention `${CMAKE_SYSTEM_NAME}.cmake` so that they can be loaded automatically from the main `CMakeLists.txt` file.
