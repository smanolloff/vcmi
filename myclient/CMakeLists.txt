add_library(myclient SHARED myclient.cpp myclient.h)
add_executable(myclient-gui main-shared.cpp main-gui.cpp main.h)
add_executable(myclient-headless main-shared.cpp main-headless.cpp main.h)

add_dependencies(myclient vcmiclient)
add_dependencies(myclient-gui myclient)
add_dependencies(myclient-headless myclient)

target_link_libraries(myclient PRIVATE vcmiclient)
target_link_libraries(myclient-gui PRIVATE myclient)
target_link_libraries(myclient-headless PRIVATE myclient)
target_include_directories(myclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(myclient-gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(myclient-headless PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(myclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(myclient-gui PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(myclient-headless PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)

vcmi_set_output_dir(myclient "")
vcmi_set_output_dir(myclient-gui "")
vcmi_set_output_dir(myclient-headless "")
enable_pch(myclient)
enable_pch(myclient-gui)
enable_pch(myclient-headless)

install(TARGETS myclient DESTINATION ${BIN_DIR})
install(TARGETS myclient-gui DESTINATION ${BIN_DIR})
install(TARGETS myclient-headless DESTINATION ${BIN_DIR})
