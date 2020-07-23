include("../build.cmake")

set(src "benchmark.cpp")

set(das_files
  "benchmark.das"
)

das_aot("${das_files}" aot_files)

add_executable(ecs-benchmark ${src} ${aot_files})
add_dependencies(ecs-benchmark ecs EASTL libDaScript)

ecs_post_build(ecs-benchmark)

target_include_directories(ecs-benchmark PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/ecs"
                          "${PROJECT_SOURCE_DIR}/libs/benchmark/include"
                          ${ecs_common_includes}
                          )

target_link_directories(ecs-benchmark PUBLIC "${PROJECT_BINARY_DIR}" ${ecs_common_libs_dir})

# target_link_libraries(ecs-benchmark PUBLIC ecs benchmark shlwapi ${ecs_common_libs})

set(libs ecs shlwapi)
ecs_link_libraries(ecs-benchmark "${libs}")

target_link_libraries(ecs-benchmark
  debug "${PROJECT_SOURCE_DIR}/libs/benchmark/src/Debug/benchmark.lib"
  optimized "${PROJECT_SOURCE_DIR}/libs/benchmark/src/Release/benchmark.lib"
)

set_target_properties(ecs-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set_target_properties(ecs-benchmark PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})