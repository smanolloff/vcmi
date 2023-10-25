add_library(myclient SHARED myclient.cpp myclient.h)
add_executable(mytest main.cpp myclient.cpp myclient.h)
add_executable(myvcmiclient main_vcmiclient.cpp main_vcmiclient.cpp)

add_dependencies(myclient vcmiclient)
add_dependencies(mytest vcmiclient)
add_dependencies(myvcmiclient vcmiclient)

target_link_libraries(myclient PRIVATE vcmiclient)
target_link_libraries(mytest PRIVATE vcmiclient)
target_link_libraries(myvcmiclient PRIVATE vcmiclient)
target_include_directories(myclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mytest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(myvcmiclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(myclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(mytest PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(myvcmiclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)

vcmi_set_output_dir(myclient "")
vcmi_set_output_dir(mytest "")
vcmi_set_output_dir(myvcmiclient "")
enable_pch(myclient)
enable_pch(mytest)
enable_pch(myvcmiclient)

install(TARGETS myclient DESTINATION ${BIN_DIR})
install(TARGETS mytest DESTINATION ${BIN_DIR})
install(TARGETS myvcmiclient DESTINATION ${BIN_DIR})
