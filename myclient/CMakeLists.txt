cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

option(ENABLE_LIBTORCH "link against libtorch for loading MMAI_MODEL Torch JIT files" ON)

add_definitions(-DVCMI_BIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_definitions(-DVCMI_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_library(myclient SHARED myclient.cpp myclient.h)

add_executable(myclient-gui main-shared.cpp main-gui.cpp main.h)
add_executable(myclient-headless main-shared.cpp main-headless.cpp main.h)

add_dependencies(myclient vcmiclient)
add_dependencies(myclient-gui myclient)
add_dependencies(myclient-headless myclient)


if(ENABLE_LIBTORCH)
  add_definitions(-DENABLE_LIBTORCH=1)

  # Ensure myclient/libtorch is a symlink to the
  # same libtorch downloaded by pip in the vcmi-gym project
  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/libtorch")
  find_package(OpenMP REQUIRED)
  find_package(Torch REQUIRED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
endif()

target_link_libraries(myclient PUBLIC vcmi vcmiclient)
target_link_libraries(myclient-gui PRIVATE myclient)
target_link_libraries(myclient-headless PRIVATE myclient)
target_include_directories(myclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(myclient-gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(myclient-headless PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(myclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(myclient-gui PRIVATE ${VCMI_LIB_TARGET})
target_link_libraries(myclient-headless PRIVATE ${VCMI_LIB_TARGET})


if(ENABLE_LIBTORCH)
  target_link_libraries(myclient PRIVATE "${TORCH_LIBRARIES}" OpenMP::OpenMP_CXX)
endif()

vcmi_set_output_dir(myclient "")
vcmi_set_output_dir(myclient-gui "")
vcmi_set_output_dir(myclient-headless "")
enable_pch(myclient)
enable_pch(myclient-gui)
enable_pch(myclient-headless)

install(TARGETS myclient DESTINATION ${BIN_DIR})
install(TARGETS myclient-gui DESTINATION ${BIN_DIR})
install(TARGETS myclient-headless DESTINATION ${BIN_DIR})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/vcmi-gym ${CMAKE_BINARY_DIR}/bin/vcmi-gym SYMBOLIC)
