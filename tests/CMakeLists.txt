include("../build.cmake")

set(src
  "tests.cpp"
  "jobmanager-unittest.cpp"
  # "query-unittest.cpp"
)

add_executable(ecs-tests ${src})
add_dependencies(ecs-tests ecs EASTL libGoogleTest)

ecs_post_build(ecs-tests)

target_include_directories(ecs-tests PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/ecs"
                          "${PROJECT_SOURCE_DIR}/libs/googletest/googletest/include"
                          ${ecs_common_includes}
                          )

target_link_directories(ecs-tests PUBLIC "${PROJECT_BINARY_DIR}" ${ecs_common_libs_dir})

set(libs ecs)
ecs_link_libraries(ecs-tests "${libs}")

target_link_libraries(ecs-tests
  debug "${CMAKE_SOURCE_DIR}/libs/googletest/googletest/lib/gtestd.lib"
  optimized "${CMAKE_SOURCE_DIR}/libs/googletest/googletest/lib/gtest.lib"
)

set_target_properties(ecs-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set_target_properties(ecs-tests PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})