cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

add_definitions(-DVCMI_BIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_definitions(-DVCMI_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_library(mlclient SHARED
  model_wrappers/function.h
  model_wrappers/function.cpp
  model_wrappers/scripted.h
  model_wrappers/scripted.cpp
  MLClient.cpp
  MLClient.h
)

add_executable(mlclient-cli
  main.cpp
  user_agents/base.h
  user_agents/agent-v1.cpp
  user_agents/agent-v1.h
  user_agents/agent-v3.cpp
  user_agents/agent-v3.h
  user_agents/agent-v4.cpp
  user_agents/agent-v4.h
)

add_dependencies(mlclient vcmiclientcommon mmailoader)
add_dependencies(mlclient-cli mlclient)

target_link_libraries(mlclient PRIVATE SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(mlclient PUBLIC vcmi vcmiclientcommon mmailoader)
target_link_libraries(mlclient-cli PRIVATE mlclient)

vcmi_set_output_dir(mlclient "")
vcmi_set_output_dir(mlclient-cli "")
enable_pch(mlclient)
enable_pch(mlclient-cli)

install(TARGETS mlclient DESTINATION ${BIN_DIR})
install(TARGETS mlclient-cli DESTINATION ${BIN_DIR})

# Convenience to enable relative paths prefixed with vcmi-gym/
# file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# file(CREATE_LINK ${CMAKE_SOURCE_DIR}/vcmi-gym ${CMAKE_BINARY_DIR}/bin/vcmi-gym SYMBOLIC)
