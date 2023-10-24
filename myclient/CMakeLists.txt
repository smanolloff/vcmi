add_library(myclient SHARED myclient.cpp myclient.h)
add_executable(mytest main.cpp main.h myclient.cpp myclient.h)

add_dependencies(myclient vcmiclient)
add_dependencies(mytest vcmiclient)

target_link_libraries(myclient PRIVATE vcmiclient)
target_link_libraries(mytest PRIVATE vcmiclient)
target_include_directories(myclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mytest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(myclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(mytest PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)

vcmi_set_output_dir(myclient "")
vcmi_set_output_dir(mytest "")
enable_pch(myclient)
enable_pch(mytest)

install(TARGETS myclient DESTINATION ${BIN_DIR})
install(TARGETS mytest DESTINATION ${BIN_DIR})
