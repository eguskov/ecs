file(GLOB src "*.cpp")

add_library(imgui ${src})

target_include_directories(imgui PUBLIC "${PROJECT_BINARY_DIR}")

set_target_properties(imgui PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})