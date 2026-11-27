cmake_minimum_required(VERSION 3.24)

# Re-enable CMP0177 after it is reset by MMAI.
if(POLICY CMP0177)
	cmake_policy(SET CMP0177 NEW)
endif()

set(MMAI_FILES
  AAI/AAI.cpp
  AAI/AAI.h

  BAI/logger.h
  BAI/factory.h
  BAI/factory.cpp
  BAI/router.cpp
  BAI/router.h
  BAI/scripted/scripted_model.h
  BAI/scripted/scripted_model.cpp
  BAI/scripted/VIPBot.h
  BAI/scripted/VIPBot.cpp
  BAI/scripted/HARBot.h
  BAI/scripted/HARBot.cpp

  BAI/v15/BAI_v15.cpp
  BAI/v15/BAI_v15.h
  BAI/v15/nn_model_v15.h
  BAI/v15/nn_model_v15.cpp
  BAI/v15/attack_log_v15.h
  BAI/v15/encoder_v15.h
  BAI/v15/enum_flags_v15.h
  BAI/v15/fastbfs_v15.h
  BAI/v15/graph/edge_store_v15.h
  BAI/v15/graph/edges/action_ends_at_hex_v15.h
  BAI/v15/graph/edges/base_v15.h
  BAI/v15/graph/edges/generic_v15.h
  BAI/v15/graph/edges/hex_adjacent_hex_v15.h
  BAI/v15/graph/edges/hex_becomes_shoot_target_after_action_v15.h
  BAI/v15/graph/edges/hex_is_end_of_action_v15.h
  BAI/v15/graph/edges/unit_acts_before_unit_v15.h
  BAI/v15/graph/edges/unit_becomes_shoot_target_after_action_v15.h
  BAI/v15/graph/edges/unit_becomes_shoot_threat_after_action_v15.h
  BAI/v15/graph/edges/unit_is_meleed_by_action_v15.h
  BAI/v15/graph/edges/unit_is_shot_by_action_v15.h
  BAI/v15/graph/edges/unit_melee_dmg_unit_v15.cpp
  BAI/v15/graph/edges/unit_melee_dmg_unit_v15.h
  BAI/v15/graph/edges/unit_shoot_dmg_unit_v15.cpp
  BAI/v15/graph/edges/unit_shoot_dmg_unit_v15.h
  BAI/v15/graph/element_v15.h
  BAI/v15/graph/graph_v15.cpp
  BAI/v15/graph/graph_v15.h
  BAI/v15/graph/node_store_v15.h
  BAI/v15/graph/nodes/action_v15.h
  BAI/v15/graph/nodes/base_v15.h
  BAI/v15/graph/nodes/global_v15.cpp
  BAI/v15/graph/nodes/global_v15.h
  BAI/v15/graph/nodes/hex_v15.cpp
  BAI/v15/graph/nodes/hex_v15.h
  BAI/v15/graph/nodes/player_v15.cpp
  BAI/v15/graph/nodes/player_v15.h
  BAI/v15/graph/nodes/unit_v15.cpp
  BAI/v15/graph/nodes/unit_v15.h
  BAI/v15/graph/util_v15.h
  BAI/v15/render_v15.cpp
  BAI/v15/render_v15.h
  BAI/v15/state_v15.cpp
  BAI/v15/state_v15.h
  BAI/v15/supplementary_data_v15.cpp
  BAI/v15/supplementary_data_v15.h
  BAI/v15/verify_v15.cpp
  BAI/v15/verify_v15.h

  BAI/v16/BAI_v16.cpp
  BAI/v16/BAI_v16.h
  BAI/v16/nn_model_v16.h
  BAI/v16/nn_model_v16.cpp
  BAI/v16/attack_log_v16.h
  BAI/v16/encoder_v16.h
  BAI/v16/enum_flags_v16.h
  BAI/v16/fastbfs_v16.h
  BAI/v16/graph/edge_store_v16.h
  BAI/v16/graph/edges/action_ends_at_hex_v16.h
  BAI/v16/graph/edges/base_v16.h
  BAI/v16/graph/edges/generic_v16.h
  BAI/v16/graph/edges/hex_adjacent_hex_v16.h
  BAI/v16/graph/edges/hex_becomes_shoot_target_after_action_v16.h
  BAI/v16/graph/edges/hex_is_end_of_action_v16.h
  BAI/v16/graph/edges/unit_acts_before_unit_v16.h
  BAI/v16/graph/edges/unit_becomes_shoot_target_after_action_v16.h
  BAI/v16/graph/edges/unit_becomes_shoot_threat_after_action_v16.h
  BAI/v16/graph/edges/unit_is_meleed_by_action_v16.h
  BAI/v16/graph/edges/unit_is_shot_by_action_v16.h
  BAI/v16/graph/edges/unit_melee_dmg_unit_v16.cpp
  BAI/v16/graph/edges/unit_melee_dmg_unit_v16.h
  BAI/v16/graph/edges/unit_shoot_dmg_unit_v16.cpp
  BAI/v16/graph/edges/unit_shoot_dmg_unit_v16.h
  BAI/v16/graph/element_v16.h
  BAI/v16/graph/graph_v16.cpp
  BAI/v16/graph/graph_v16.h
  BAI/v16/graph/node_store_v16.h
  BAI/v16/graph/nodes/action_v16.h
  BAI/v16/graph/nodes/base_v16.h
  BAI/v16/graph/nodes/global_v16.cpp
  BAI/v16/graph/nodes/global_v16.h
  BAI/v16/graph/nodes/hex_v16.cpp
  BAI/v16/graph/nodes/hex_v16.h
  BAI/v16/graph/nodes/player_v16.cpp
  BAI/v16/graph/nodes/player_v16.h
  BAI/v16/graph/nodes/unit_v16.cpp
  BAI/v16/graph/nodes/unit_v16.h
  BAI/v16/graph/util_v16.h
  BAI/v16/render_v16.cpp
  BAI/v16/render_v16.h
  BAI/v16/state_v16.cpp
  BAI/v16/state_v16.h
  BAI/v16/supplementary_data_v16.cpp
  BAI/v16/supplementary_data_v16.h
  BAI/v16/verify_v16.cpp
  BAI/v16/verify_v16.h

  schema/base.h
  schema/schema.h
  schema/v15/constants.h
  schema/v15/encoding.h
  schema/v15/graph.h
  schema/v15/schema.h
  schema/v15/types.h
  schema/v15/util.h
  schema/v16/constants.h
  schema/v16/encoding.h
  schema/v16/graph.h
  schema/v16/schema.h
  schema/v16/types.h
  schema/v16/util.h

  MMAI.h
  StdInc.h
  common.h
)

set(MMAI_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DUSING_ONNX=1)
assign_source_group(${MMAI_FILES})

add_library(MMAI OBJECT ${MMAI_FILES})
target_compile_definitions(MMAI PRIVATE VCMI_DLL=1)

target_link_libraries(MMAI PRIVATE vcmiMain)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # First try CMake config (works on Fedora, Debian, Ubuntu, Arch)
  find_package(onnxruntime CONFIG QUIET)
  if(onnxruntime_FOUND)
    message(STATUS "Found onnxruntime via CMake config")
    target_link_libraries(MMAI PRIVATE onnxruntime::onnxruntime)
  else()
    # Fallback to manual search for custom installs
    message(STATUS "onnxruntime CMake config not found, trying system installation")
    set(ONNXRUNTIME_ROOT "/opt/onnxruntime" CACHE PATH "Path to onnxruntime containing lib/onnxruntime.so and include/onnxruntime_cxx_api.h")
    message(STATUS "Looking for onnxruntime via find_path/find_library (hint: ${ONNXRUNTIME_ROOT})")
    find_path(ONNXRUNTIME_INCLUDE_DIR
      NAMES onnxruntime_cxx_api.h
      HINTS "${ONNXRUNTIME_ROOT}/include"
      PATH_SUFFIXES
          onnxruntime
          include
          include/onnxruntime
    )

    find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime libonnxruntime
      HINTS "${ONNXRUNTIME_ROOT}/lib"
    )

    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
      message(STATUS "Using onnxruntime:")
      message(STATUS "  include: ${ONNXRUNTIME_INCLUDE_DIR}")
      message(STATUS "  library: ${ONNXRUNTIME_LIBRARY}")
      target_include_directories(MMAI PRIVATE "${ONNXRUNTIME_INCLUDE_DIR}")
      target_link_libraries(MMAI PRIVATE "${ONNXRUNTIME_LIBRARY}")
    else()
      message(FATAL_ERROR "onnxruntime not found. Install onnxruntime-devel or set ONNXRUNTIME_ROOT to a custom installation path, or disable MMAI with -DENABLE_MMAI=OFF")
    endif()
  endif()
else()
  find_package(onnxruntime CONFIG REQUIRED)
  target_link_libraries(MMAI PRIVATE onnxruntime::onnxruntime)
endif()

# for fallback & spell casting
target_link_libraries(MMAI PRIVATE BattleAI StupidAI)

# Used in schema/base.h to determine if it is imported by MMAI or vcmi-gym
target_compile_definitions(MMAI PRIVATE MMAI_DLL=1)
target_include_directories(MMAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(MMAI "AI")
enable_pch(MMAI)
