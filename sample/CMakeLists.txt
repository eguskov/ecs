configure_file(sample-config.h.in sample-config.h @ONLY)

include("../build.cmake")

set(source_files
  "boids.h"
  "boids.cpp"
  "grid.cpp"
  "physics.cpp"
  "triggers.cpp"
  "update.h"
  "update.cpp"
)

ecs_add_codegen("${source_files}" gen_files)

add_executable(sample sample.cpp ${gen_files})
add_dependencies(sample ecs EASTL raylib box2d)

ecs_post_build(sample)

set(libs ecs Winmm)
ecs_link_libraries(sample "${libs}")

target_link_libraries(sample
  debug "${PROJECT_BINARY_DIR}/libs/raylib/src/raylib_static-dbg.lib"
  debug "${PROJECT_BINARY_DIR}/libs/Box2D/src/box2d-dbg.lib"
  optimized "${PROJECT_BINARY_DIR}/libs/raylib/src/raylib_static.lib"
  optimized "${PROJECT_BINARY_DIR}/libs/Box2D/src/box2d.lib"
)

target_link_directories(sample PUBLIC
                        "${PROJECT_BINARY_DIR}"
                        "${PROJECT_SOURCE_DIR}/Debug"
                        ${ecs_common_libs_dir}
                       )

target_include_directories(sample PUBLIC
                          "${CMAKE_BINARY_DIR}/sample"
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/ecs"
                          "${PROJECT_SOURCE_DIR}/libs/raylib/src"
                          "${PROJECT_SOURCE_DIR}/libs/Box2D"
                          "${PROJECT_SOURCE_DIR}/libs/raygui/src"
                          ${ecs_common_includes}
                          )

set_target_properties(sample PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set_target_properties(sample PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})