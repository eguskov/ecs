cmake_minimum_required(VERSION 3.0.0)

project(root VERSION 1.0)

set(CMAKE_DEBUG_POSTFIX "-dbg")

add_definitions("/FC")

include(ExternalProject)
ExternalProject_Add(
    libDaScript 
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/daScript"
    PREFIX "libs/daScript"
    STAMP_DIR ""
    BUILD_IN_SOURCE TRUE
    BUILD_ALWAYS TRUE
    LOG_BUILD TRUE
    LOG_MERGED_STDOUTERR TRUE
    INSTALL_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    BUILD_COMMAND 
        cmake.exe --build . --config ${CMAKE_BUILD_TYPE} --target all
    STEP_TARGETS build)

add_subdirectory(libs/EASTL)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/raylib)

set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/Box2D)

add_subdirectory(codegen)

add_subdirectory(ecs)
add_subdirectory(sample)
add_subdirectory(benchmark)