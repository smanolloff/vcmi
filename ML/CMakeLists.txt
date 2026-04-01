cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

add_definitions(-DVCMI_BIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_definitions(-DVCMI_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_library(mlclient SHARED
  model_wrappers/function.h
  model_wrappers/function.cpp
  model_wrappers/scripted.h
  model_wrappers/scripted.cpp
  model_wrappers/path.h
  model_wrappers/path.cpp
  MLClient.cpp
  MLClient.h
)

add_executable(mlclient-cli
  main.cpp
  user_agents/base.h
  user_agents/agent-v13.cpp
  user_agents/agent-v13.h
)

add_dependencies(mlclient-cli mlclient)
target_include_directories(mlclient PUBLIC "${CMAKE_SOURCE_DIR}/AI/MMAI")
target_link_libraries(mlclient PRIVATE SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(mlclient PUBLIC vcmi vcmiclientcommon)
target_link_libraries(mlclient-cli PRIVATE mlclient)

vcmi_set_output_dir(mlclient "")
vcmi_set_output_dir(mlclient-cli "")
enable_pch(mlclient)
enable_pch(mlclient-cli)

install(TARGETS mlclient DESTINATION ${BIN_DIR})
install(TARGETS mlclient-cli DESTINATION ${BIN_DIR})

add_custom_command(
    TARGET mlclient          # Replace with the actual target name
    POST_BUILD               # Execute after the target is built
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_BINARY_DIR}/bin/data
    COMMENT "symlink ${CMAKE_BINARY_DIR}/bin/data -> ${CMAKE_SOURCE_DIR}/data"
)
