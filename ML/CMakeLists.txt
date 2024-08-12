cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

add_definitions(-DVCMI_BIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_definitions(-DVCMI_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_library(mlclient SHARED MLClient.cpp MLClient.h)

set(SHARED_FILES
  main.h
  main-shared.cpp
  user_agents/base.h
  user_agents/agent-v1.cpp
  user_agents/agent-v1.h
  user_agents/agent-v3.cpp
  user_agents/agent-v3.h
)

add_executable(mlclient-gui ${SHARED_FILES} main-gui.cpp)
add_executable(mlclient-headless ${SHARED_FILES} main-headless.cpp)

add_dependencies(mlclient vcmiclient mlclientplugin)
add_dependencies(mlclient-gui mlclient)
add_dependencies(mlclient-headless mlclient)

target_link_libraries(mlclient PRIVATE SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(mlclient PUBLIC vcmi vcmiclient mlclientplugin)
target_link_libraries(mlclient-gui PRIVATE mlclient)
target_link_libraries(mlclient-headless PRIVATE mlclient)

vcmi_set_output_dir(mlclient "")
vcmi_set_output_dir(mlclient-gui "")
vcmi_set_output_dir(mlclient-headless "")
enable_pch(mlclient)
enable_pch(mlclient-gui)
enable_pch(mlclient-headless)

install(TARGETS mlclient DESTINATION ${BIN_DIR})
install(TARGETS mlclient-gui DESTINATION ${BIN_DIR})
install(TARGETS mlclient-headless DESTINATION ${BIN_DIR})

# Convenience to enable relative paths prefixed with vcmi-gym/
# file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# file(CREATE_LINK ${CMAKE_SOURCE_DIR}/vcmi-gym ${CMAKE_BINARY_DIR}/bin/vcmi-gym SYMBOLIC)
