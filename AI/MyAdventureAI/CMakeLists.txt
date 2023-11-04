set(myAdventureAI_SRCS MyAdventureAI.cpp)
set(myAdventureAI_HEADERS StdInc.h MyAdventureAI.h)

if(NOT ENABLE_STATIC_AI_LIBS)
  list(APPEND myAdventureAI_SRCS main.cpp StdInc.cpp)
endif()
assign_source_group(${myAdventureAI_SRCS} ${myAdventureAI_HEADERS})

if(ENABLE_STATIC_AI_LIBS)
  add_library(MyAdventureAI STATIC ${myAdventureAI_SRCS} ${myAdventureAI_HEADERS})
else()
  add_library(MyAdventureAI SHARED ${myAdventureAI_SRCS} ${myAdventureAI_HEADERS})
  install(TARGETS MyAdventureAI RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

target_link_libraries(MyAdventureAI PRIVATE ${VCMI_LIB_TARGET})
target_include_directories(MyAdventureAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MyAdventureAI "AI")
enable_pch(MyAdventureAI)
