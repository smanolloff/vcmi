cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

option(ENABLE_LIBTORCH "link against libtorch to enable loading of MMAI_MODEL TorchJIT files" ON)

add_definitions(-DVCMI_BIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_definitions(-DVCMI_ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_library(gymclient SHARED gymclient.cpp gymclient.h)

add_executable(gymclient-gui main-shared.cpp main-gui.cpp main.h)
add_executable(gymclient-headless main-shared.cpp main-headless.cpp main.h)

add_dependencies(gymclient vcmiclient)
add_dependencies(gymclient-gui gymclient)
add_dependencies(gymclient-headless gymclient)


if(ENABLE_LIBTORCH)
  add_definitions(-DENABLE_LIBTORCH=1)

  # Ensure gymclient/libtorch is a symlink to the
  # same libtorch downloaded by pip in the vcmi-gym project
  # (e.g. "VCMI_GYM_DIR/.venv/lib/python3.10/site-packages/torch")

  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libtorch")
  find_package(OpenMP REQUIRED)
  find_package(Torch REQUIRED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
endif()

target_link_libraries(gymclient PUBLIC vcmi vcmiclient)
target_link_libraries(gymclient-gui PRIVATE gymclient)
target_link_libraries(gymclient-headless PRIVATE gymclient)

# target_include_directories(gymclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_include_directories(gymclient-gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_include_directories(gymclient-headless PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(gymclient PRIVATE ${VCMI_LIB_TARGET} SDL2::SDL2 SDL2::Image SDL2::Mixer SDL2::TTF)
target_link_libraries(gymclient-gui PRIVATE ${VCMI_LIB_TARGET})
target_link_libraries(gymclient-headless PRIVATE ${VCMI_LIB_TARGET})


if(ENABLE_LIBTORCH)
  target_link_libraries(gymclient PRIVATE "${TORCH_LIBRARIES}" OpenMP::OpenMP_CXX)
endif()

vcmi_set_output_dir(gymclient "")
vcmi_set_output_dir(gymclient-gui "")
vcmi_set_output_dir(gymclient-headless "")
enable_pch(gymclient)
enable_pch(gymclient-gui)
enable_pch(gymclient-headless)

install(TARGETS gymclient DESTINATION ${BIN_DIR})
install(TARGETS gymclient-gui DESTINATION ${BIN_DIR})
install(TARGETS gymclient-headless DESTINATION ${BIN_DIR})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/vcmi-gym ${CMAKE_BINARY_DIR}/bin/vcmi-gym SYMBOLIC)
