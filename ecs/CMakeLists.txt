file(GLOB ecs_src "ecs/*.cpp")
file(GLOB components_src "components/*.cpp")

list(APPEND ecs_src ${components_src})

add_library(ecs ${ecs_src})
add_dependencies(ecs EASTL)

target_include_directories(ecs PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/ecs"
                          "${PROJECT_SOURCE_DIR}/libs/EASTL/include"
                          "${PROJECT_SOURCE_DIR}/libs/EASTL/test/packages/EABase/include/Common"
                          "${PROJECT_SOURCE_DIR}/libs/glm"
                          "${PROJECT_SOURCE_DIR}/libs/yaml/include"
                          "${PROJECT_SOURCE_DIR}/libs/libyaml/include"
                          "${PROJECT_SOURCE_DIR}/libs/jemalloc/include"
                          "${PROJECT_SOURCE_DIR}/libs/jemalloc/include/msvc_compat"
                          "${PROJECT_SOURCE_DIR}/../daScript/include"
                          )

set_target_properties(ecs PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})