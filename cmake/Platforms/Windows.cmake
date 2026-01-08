add_definitions(-DWIN32 -D_WINDOWS)

option(WINDOWS_USE_RESOURCE_FILE "Use resource file (Windows)" ON)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" ON)
else()
  option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" OFF)
endif()

option(USING_DIRECT3D "Use Direct3D" OFF)

if(USING_DIRECT3D)
  add_definitions(-DUSING_DIRECT3D)
endif()

set(WIN_RES_FILE "${CMAKE_SOURCE_DIR}/meta/win/HatchGameEngine.rc")

if(RESOURCE_FILE)
  set(WIN_RES_FILE ${RESOURCE_FILE})
endif()

set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/meta/win/include/SDL2/")

if(USING_OPENGL)
  add_definitions(-DGLEW_STATIC)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(USE_OPEN_ASSET_IMPORT_LIBRARY OFF)

  if(USING_OPENGL)
    set(OPENGL_LIBRARIES "${CMAKE_SOURCE_DIR}/meta/win/lib/msvc/x64/glew32s.lib")
  endif()
else()
  if(USE_OPEN_ASSET_IMPORT_LIBRARY)
    set(assimp_FOUND TRUE)
    set(ASSIMP_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/meta/win/include/assimp/")
    set(ASSIMP_LIBRARIES "${CMAKE_SOURCE_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libassimp.a;${CMAKE_SOURCE_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libzlibstatic.a")
  endif()

  if(USING_OPENGL)
    set(OPENGL_LIBRARIES "${CMAKE_SOURCE_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libglew32.a;-lgdi32;-lopengl32")
  endif()
endif()

# Add all of the Windows include files
include_directories("${CMAKE_SOURCE_DIR}/meta/win/include")

if(WINDOWS_USE_RESOURCE_FILE)
  target_sources(${PROJECT_NAME} PRIVATE ${WIN_RES_FILE})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_link_libraries(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/meta/win/lib/msvc/x64/SDL2.lib"
    Ws2_32.lib opengl32.lib winmm.lib imm32.lib version.lib setupapi.lib)
else()
  target_link_libraries(${PROJECT_NAME}
    "${CMAKE_SOURCE_DIR}/meta/win/lib/mingw/${TOOLCHAIN_PREFIX}/libSDL2.a"
    -static -lmingw32 -lm -ldinput8 -ldxguid -ldxerr8
    -luser32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32
    -lversion -luuid -lhid -lsetupapi -lws2_32)

  if(NOT WINDOWS_COMPILE_AS_CONSOLE_APP)
    target_link_libraries(${PROJECT_NAME} -mwindows)
  endif()
endif()
add_definitions(-DSDL_MAIN_HANDLED)
