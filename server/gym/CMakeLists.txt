cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

add_library(gymserverplugin SHARED
  # ServerPlugin.cpp
  ServerPlugin.h
)

target_link_libraries(MMAI PUBLIC vcmi)

vcmi_set_output_dir(gymserverplugin "")
enable_pch(gymserverplugin)
