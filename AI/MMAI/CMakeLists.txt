set(MMAI_FILES
  AAI/AAI.h
  AAI/AAI.cpp
  AAI/impl.cpp

  BAI/build_action_result.cpp
  BAI/build_action_result.h
  BAI/BAI.cpp
  BAI/BAI.h
  BAI/impl.cpp
  BAI/init.cpp
  BAI/util.cpp

  types/action.cpp
  types/action.h
  types/battlefield.cpp
  types/battlefield.h
  types/hex.cpp
  types/hex.h
  types/hexaction.h
  types/hexactmask.h
  types/hexstate.h
  types/stack.cpp
  types/stack.h

  include/loader.h
  common.h
  MMAI.h
  export.h
  StdInc.h
)

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

target_link_libraries(MMAI PRIVATE ${VCMI_LIB_TARGET} pybind11::headers pybind11::pybind11 pybind11::module pybind11::lto)
target_include_directories(MMAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MMAI "AI")
enable_pch(MMAI)
