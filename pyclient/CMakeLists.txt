add_library(pyclient SHARED pyclient.cpp pyclient.h)
add_executable(pytest main.cpp aitypes.h main.h pyclient.cpp pyclient.h)

add_dependencies(pyclient vcmiclient)
add_dependencies(pytest vcmiclient)

target_link_libraries(pyclient PRIVATE vcmiclient)
target_link_libraries(pytest PRIVATE vcmiclient)
target_include_directories(pyclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pytest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pyclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(pytest PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)

vcmi_set_output_dir(pyclient "")
vcmi_set_output_dir(pytest "")
enable_pch(pyclient)
enable_pch(pytest)

install(TARGETS pyclient DESTINATION ${BIN_DIR})
install(TARGETS pytest DESTINATION ${BIN_DIR})
