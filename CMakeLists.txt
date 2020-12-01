cmake_minimum_required(VERSION 3.0.0)

project(root VERSION 1.0)

set(CMAKE_DEBUG_POSTFIX "-dbg")

add_definitions("/FC")

include("./daScript.cmake")
include("./tests/gTest.cmake")

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
add_subdirectory(tests)