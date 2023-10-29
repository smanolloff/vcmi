set(myBattleAI_SRCS MyBattleAI.cpp)
set(myBattleAI_HEADERS StdInc.h MyBattleAI.h)

if(NOT ENABLE_STATIC_AI_LIBS)
  list(APPEND myBattleAI_SRCS main.cpp StdInc.cpp)
endif()
assign_source_group(${myBattleAI_SRCS} ${myBattleAI_HEADERS})

if(ENABLE_STATIC_AI_LIBS)
  add_library(MyBattleAI STATIC ${myBattleAI_SRCS} ${myBattleAI_HEADERS})
else()
  add_library(MyBattleAI SHARED ${myBattleAI_SRCS} ${myBattleAI_HEADERS})
  install(TARGETS MyBattleAI RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

target_link_libraries(MyBattleAI PRIVATE ${VCMI_LIB_TARGET})
target_include_directories(MyBattleAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MyBattleAI "AI")
enable_pch(MyBattleAI)
