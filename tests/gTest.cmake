include(ExternalProject)
ExternalProject_Add(
  googletest 
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/googletest/googletest"
  PREFIX "libs/googletest/googletest"
  STAMP_DIR ""
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS 1
  LOG_BUILD 1
  LOG_MERGED_STDOUTERR 1
  INSTALL_COMMAND ""
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DGOOGLETEST_VERSION:STRING=1.10.0
    -DBUILD_SHARED_LIBS=NO
    -Dgtest_force_shared_crt=YES
  BUILD_COMMAND 
    cmake.exe --build . --config ${CMAKE_BUILD_TYPE} --target all
  STEP_TARGETS build
)

add_custom_target(
  libGoogleTest
  DEPENDS googletest
  COMMAND echo "Building google-test..."
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/libs/googletest/googletest"
  BYPRODUCTS "${CMAKE_SOURCE_DIR}/libs/googletest/googletest/lib/gtestd.lib" "${CMAKE_SOURCE_DIR}/libs/googletest/googletest/lib/gtest.lib"
)