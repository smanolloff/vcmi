# TODO: re-use client_SRCS and simply remove CMT file, adding mine instead

add_executable(myclient myclient.cpp myclient.h)
add_dependencies(myclient vcmiclient)

target_link_libraries(myclient PRIVATE vcmiclient)
target_include_directories(myclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(myclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)

vcmi_set_output_dir(myclient "")
enable_pch(myclient)

install(TARGETS myclient DESTINATION ${BIN_DIR})
