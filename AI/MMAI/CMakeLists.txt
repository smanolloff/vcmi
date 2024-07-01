set(MMAI_FILES
  AAI/AAI.cpp
  AAI/AAI.h


  BAI/base.cpp
  BAI/base.h
  BAI/router.cpp
  BAI/router.h
  BAI/scripted/summoner.cpp
  BAI/scripted/summoner.h

  BAI/v1/BAI.cpp
  BAI/v1/BAI.h
  BAI/v1/action.cpp
  BAI/v1/action.h
  BAI/v1/attack_log.h
  BAI/v1/battlefield.cpp
  BAI/v1/battlefield.h
  BAI/v1/encoder.cpp
  BAI/v1/encoder.h
  BAI/v1/hex.cpp
  BAI/v1/hex.h
  BAI/v1/hexaction.h
  BAI/v1/hexactmask.h
  BAI/v1/render.cpp
  BAI/v1/render.h
  BAI/v1/stackinfo.h
  BAI/v1/state.cpp
  BAI/v1/state.h
  BAI/v1/supplementary_data.cpp
  BAI/v1/supplementary_data.h

  BAI/v2/BAI.cpp
  BAI/v2/BAI.h
  BAI/v2/encoder.cpp
  BAI/v2/encoder.h
  BAI/v2/state.cpp
  BAI/v2/state.h

  BAI/v3/BAI.cpp
  BAI/v3/BAI.h
  BAI/v3/action.cpp
  BAI/v3/action.h
  BAI/v3/attack_log.h
  BAI/v3/battlefield.cpp
  BAI/v3/battlefield.h
  BAI/v3/encoder.cpp
  BAI/v3/encoder.h
  BAI/v3/general_info.cpp
  BAI/v3/general_info.h
  BAI/v3/hex.cpp
  BAI/v3/hex.h
  BAI/v3/hexaction.h
  BAI/v3/hexactmask.h
  BAI/v3/render.cpp
  BAI/v3/render.h
  BAI/v3/stack.cpp
  BAI/v3/stack.h
  BAI/v3/state.cpp
  BAI/v3/state.h
  BAI/v3/supplementary_data.cpp
  BAI/v3/supplementary_data.h

  schema/schema.h
  schema/v1/constants.h
  schema/v1/schema.h
  schema/v1/types.h
  schema/v1/util.h
  schema/v2/constants.h
  schema/v2/schema.h
  schema/v3/constants.h
  schema/v3/schema.h
  schema/v3/types.h
  schema/v3/util.h

  MMAI.h
  StdInc.h
  common.h
)

option(ENABLE_MMAI_TEST "Compile tests" OFF)

if(NOT ENABLE_STATIC_AI_LIBS)
  list(APPEND MMAI_FILES main.cpp StdInc.cpp)
endif()
assign_source_group(${MMAI_FILES})

if(ENABLE_STATIC_AI_LIBS)
  add_library(MMAI STATIC ${MMAI_FILES})
else()
  add_library(MMAI SHARED ${MMAI_FILES})
  install(TARGETS MMAI RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

set_target_properties(MMAI PROPERTIES COMPILE_DEFINITIONS "MMAI_DLL=1")
target_link_libraries(MMAI PUBLIC vcmi)
target_include_directories(MMAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ENABLE_MMAI_TEST)
  include(GoogleTest)
  include(CheckCXXCompilerFlag)
  enable_testing()

  target_link_libraries(MMAI PUBLIC gtest gtest_main)

  target_include_directories(MMAI PRIVATE "${CMAKE_SOURCE_DIR}/test/googletest/googletest/include")
  add_subdirectory(${CMAKE_SOURCE_DIR}/test/googletest ${CMAKE_SOURCE_DIR}/test/googletest/build EXCLUDE_FROM_ALL)
  add_executable(MMAI_test test/encoder_test.cpp)
  target_link_libraries(MMAI_test PRIVATE MMAI)
  gtest_discover_tests(MMAI_test)

  # default visibility is needed for testing
  set_target_properties(MMAI PROPERTIES CXX_VISIBILITY_PRESET "default")
  set_target_properties(MMAI_test PROPERTIES CXX_VISIBILITY_PRESET "default")

  # Run tests with:
  # ctest --test-dir build/AI/MMAI/
endif()

vcmi_set_output_dir(MMAI "AI")
enable_pch(MMAI)
