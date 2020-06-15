set(ecs_common_includes
  "${PROJECT_SOURCE_DIR}/libs/EASTL/include"
  "${PROJECT_SOURCE_DIR}/libs/EASTL/test/packages/EABase/include/Common"
  "${PROJECT_SOURCE_DIR}/libs/glm"
  "${PROJECT_SOURCE_DIR}/libs/raylib/src"
  "${PROJECT_SOURCE_DIR}/libs/log4cplus/include"
  "${PROJECT_SOURCE_DIR}/libs/Box2D/include"
  "${PROJECT_SOURCE_DIR}/../daScript/include"
)

set(ecs_common_libs_dir
  "${PROJECT_SOURCE_DIR}/libs/log4cplus/msvc14/Win32/bin.Release"
)

set(ecs_common_libs log4cplus)
set(
  ecs_common_libs_debug
  ${ecs_common_libs}
  DbgHelp
  "${PROJECT_BINARY_DIR}/libs/EASTL/EASTL-dbg.lib"
  "${PROJECT_SOURCE_DIR}/libs/jemalloc/msvc/Win32/Debug/jemallocd.lib"
  "${PROJECT_SOURCE_DIR}/../daScript/build/libDaScript.lib")

set(
  ecs_common_libs_release
  ${ecs_common_libs}
  "${PROJECT_BINARY_DIR}/libs/EASTL/EASTL.lib"
  "${PROJECT_SOURCE_DIR}/libs/jemalloc/msvc/Win32/Release/jemalloc.lib"
  "${PROJECT_SOURCE_DIR}/../daScript/build/libDaScript.lib")

function(ecs_add_codegen source_files ret)
  set(codegen_exe "${PROJECT_SOURCE_DIR}/bin/codegen.exe")

  foreach(src IN LISTS source_files)
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    set(target_file "${CMAKE_CURRENT_SOURCE_DIR}/${src}.gen")

    add_custom_command(
      OUTPUT ${target_file}
      COMMAND ${codegen_exe} ${source_file} ${target_file}
      DEPENDS codegen ${source_file}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Do codegen for ${src}..."
    )

    list(APPEND result ${target_file})

    set_source_files_properties(${target_file} PROPERTIES LANGUAGE CXX)
  endforeach()

  SET(${ret} ${result} PARENT_SCOPE)
endfunction()

function(ecs_post_build trg)
  add_custom_command(
    TARGET ${trg} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/Debug/jemallocd.dll" "${PROJECT_SOURCE_DIR}/bin/jemallocd.dll"
    COMMENT "Copy jemallocd.dll"
  )
  add_custom_command(
    TARGET ${trg} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/Release/jemalloc.dll" "${PROJECT_SOURCE_DIR}/bin/jemalloc.dll"
    COMMENT "Copy jemalloc.dll"
  )
  add_custom_command(
    TARGET ${trg} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/Release/log4cplus.dll" "${PROJECT_SOURCE_DIR}/bin/log4cplus.dll"
    COMMENT "Copy log4cplus.dll"
  )
endfunction()

function(ecs_link_libraries trg libs)
  foreach(lib IN LISTS libs)
    target_link_libraries(${trg} debug ${lib})
    target_link_libraries(${trg} optimized ${lib})
  endforeach()

  foreach(lib IN LISTS ecs_common_libs_debug)
    target_link_libraries(${trg} debug ${lib})
  endforeach()

  foreach(lib IN LISTS ecs_common_libs_release)
    target_link_libraries(${trg} optimized ${lib})
  endforeach()
endfunction()

function(das_aot source_files ret)
  set(das_aot_exe "${PROJECT_SOURCE_DIR}/../daScript/build/dasAot.exe")

  foreach(src IN LISTS source_files)
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    set(target_file "${CMAKE_CURRENT_SOURCE_DIR}/${src}.aot.cpp")

    add_custom_command(
      OUTPUT ${target_file}
      COMMAND ${das_aot_exe} ${source_file} ${target_file}
      DEPENDS ${source_file}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "AOT for ${src}..."
    )

    list(APPEND result ${target_file})
  endforeach()

  SET(${ret} ${result} PARENT_SCOPE)
endfunction()