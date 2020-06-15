file(GLOB codegen_src "*.cpp")

add_executable(codegen ${codegen_src})
add_dependencies(codegen EASTL)

target_include_directories(codegen PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/libs/fmt/include"
                          "${PROJECT_SOURCE_DIR}/libs/EASTL/include"
                          "${PROJECT_SOURCE_DIR}/libs/EASTL/test/packages/EABase/include/Common"
                          "${PROJECT_SOURCE_DIR}/libs/LLVM/include"
                          "${PROJECT_SOURCE_DIR}/libs/PEGTL/include/tao")

target_link_directories(codegen PUBLIC
                        "${PROJECT_BINARY_DIR}"
                        "${PROJECT_SOURCE_DIR}/libs/LLVM/lib")

target_link_libraries(codegen debug libclang)
target_link_libraries(codegen debug "${PROJECT_BINARY_DIR}/libs/EASTL/EASTL-dbg.lib")
target_link_libraries(codegen debug "${PROJECT_SOURCE_DIR}/libs/fmt/Debug/fmtd.lib")

target_link_libraries(codegen optimized libclang)
target_link_libraries(codegen optimized "${PROJECT_BINARY_DIR}/libs/EASTL/EASTL.lib")
target_link_libraries(codegen optimized "${PROJECT_SOURCE_DIR}/libs/fmt/Release/fmt.lib")

set_target_properties(codegen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
# set_target_properties(codegen PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_target_properties(codegen PROPERTIES DEBUG_POSTFIX "")