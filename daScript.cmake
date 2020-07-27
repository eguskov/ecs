include(ExternalProject)
ExternalProject_Add(
  daScript 
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/daScript"
  PREFIX "libs/daScript"
  STAMP_DIR ""
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS 1
  LOG_BUILD 1
  LOG_MERGED_STDOUTERR 1
  INSTALL_COMMAND ""
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  BUILD_COMMAND 
    cmake.exe --build . --config ${CMAKE_BUILD_TYPE} --target all
  STEP_TARGETS build
)

add_custom_target(
  libDaScript
  DEPENDS daScript
  COMMAND echo "Building daScript..."
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/libs/daScript"
  BYPRODUCTS "${CMAKE_SOURCE_DIR}/libs/daScript/libDaScript.lib"
)