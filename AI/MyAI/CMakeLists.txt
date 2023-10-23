set(myAI_SRCS
    MyAI.cpp
)

set(myAI_HEADERS
    StdInc.h

    MyAI.h
)

if(NOT ENABLE_STATIC_AI_LIBS)
  list(APPEND myAI_SRCS main.cpp StdInc.cpp)
endif()
assign_source_group(${myAI_SRCS} ${myAI_HEADERS})

if(ENABLE_STATIC_AI_LIBS)
  add_library(MyAI STATIC ${myAI_SRCS} ${myAI_HEADERS})
else()
  add_library(MyAI SHARED ${myAI_SRCS} ${myAI_HEADERS})
  install(TARGETS MyAI RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

target_link_libraries(MyAI PRIVATE ${VCMI_LIB_TARGET})
target_include_directories(MyAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MyAI "AI")
enable_pch(MyAI)
