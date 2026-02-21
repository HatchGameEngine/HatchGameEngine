add_executable(${PROJECT_NAME} ${HATCH_SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DWIN32 -D_WINDOWS)

option(WINDOWS_USE_RESOURCE_FILE "Use resource file (Windows)" ON)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" ON)
else()
  option(WINDOWS_COMPILE_AS_CONSOLE_APP "Compile as a console application" OFF)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4267 /wd4305 /wd4717 /EHsc")
endif()

option(USING_DIRECT3D "Use Direct3D" OFF)

if(USING_DIRECT3D)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSING_DIRECT3D)
endif()

set(WIN_RES_FILE "${CMAKE_SOURCE_DIR}/meta/win/HatchGameEngine.rc")

if(RESOURCE_FILE)
  set(WIN_RES_FILE ${RESOURCE_FILE})
endif()

if(USING_OPENGL)
  include(cmake/Dependencies/FetchGLEW.cmake)
endif()

if(WINDOWS_USE_RESOURCE_FILE)
  target_sources(${PROJECT_NAME} PRIVATE ${WIN_RES_FILE})
endif()

include(cmake/Dependencies/FetchSDL2.cmake)

if(USE_OPEN_ASSET_IMPORT_LIBRARY)
  include(cmake/Dependencies/Fetchassimp.cmake)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_link_libraries(${PROJECT_NAME}
    Ws2_32.lib opengl32.lib winmm.lib imm32.lib version.lib setupapi.lib)
else()
  target_link_libraries(${PROJECT_NAME}
    -static -lmingw32 -lm -ldinput8 -ldxguid -ldxerr8
    -luser32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32
    -lversion -luuid -lhid -lsetupapi -lws2_32)

  if(NOT WINDOWS_COMPILE_AS_CONSOLE_APP)
    target_link_libraries(${PROJECT_NAME} -mwindows)
  endif()
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE -DSDL_MAIN_HANDLED)
