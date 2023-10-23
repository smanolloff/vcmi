if(ENABLE_MYCLIENT_LIB)
  add_library(myclient SHARED myclient.cpp myclient.h)
else()
  add_executable(myclient main.cpp main.h myclient.cpp myclient.h)
endif()

add_dependencies(myclient vcmiclient)

target_link_libraries(myclient PRIVATE vcmiclient)
target_include_directories(myclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(myclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)

vcmi_set_output_dir(myclient "")
enable_pch(myclient)

install(TARGETS myclient DESTINATION ${BIN_DIR})
