set(MMAI_FILES
  AAI/AAI.cpp
  AAI/AAI.h

  BAI/BAI.cpp
  BAI/BAI.h
  BAI/delegatee.cpp
  BAI/delegatee.h
  BAI/v1/BAI.cpp
  BAI/v1/BAI.h
  BAI/v1/action.cpp
  BAI/v1/action.h
  BAI/v1/attack_log.h
  BAI/v1/battlefield.cpp
  BAI/v1/battlefield.h
  BAI/v1/encode.cpp
  BAI/v1/encode.h
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

  schema/schema.h
  schema/v1/constants.h
  schema/v1/types.h

  MMAI.h
  StdInc.h
  common.h
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

set_target_properties(MMAI PROPERTIES COMPILE_DEFINITIONS "MMAI_DLL=1")

target_link_libraries(MMAI PRIVATE vcmi)
target_include_directories(MMAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MMAI "AI")
enable_pch(MMAI)
