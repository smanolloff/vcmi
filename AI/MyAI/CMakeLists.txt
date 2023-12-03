set(MyAI_FILES
  AAI/AAI.h
  AAI/AAI.cpp

  BAI/build_action_result.cpp
  BAI/build_action_result.h
  BAI/BAI.cpp
  BAI/BAI.h
  BAI/impl.cpp
  BAI/init.cpp
  BAI/util.cpp

  types/action.cpp
  types/action.h
  types/action_enums.h
  types/battlefield.cpp
  types/battlefield.h
  types/hex.cpp
  types/hex.h
  types/hexactmask.h
  types/hexstate.h

  common.h
  MyAI.h
  mytypes.h
  StdInc.h
)

if(NOT ENABLE_STATIC_AI_LIBS)
  list(APPEND MyAI_FILES main.cpp StdInc.cpp)
endif()
assign_source_group(${MyAI_FILES})

if(ENABLE_STATIC_AI_LIBS)
  add_library(MyAI STATIC ${MyAI_FILES})
else()
  add_library(MyAI SHARED ${MyAI_FILES})
  install(TARGETS MyAI RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

target_link_libraries(MyAI PRIVATE ${VCMI_LIB_TARGET} pybind11::headers pybind11::pybind11 pybind11::module pybind11::lto)
target_include_directories(MyAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MyAI "AI")
enable_pch(MyAI)
