cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

find_package(SQLite3 REQUIRED)

add_library(gymserverplugin SHARED
  GymInfo.h
  ServerPlugin.cpp
  ServerPlugin.h
  Stats.cpp
  Stats.h

  StdInc.h
)

add_dependencies(gymserverplugin vcmi)
target_link_libraries(gymserverplugin PRIVATE vcmi SQLite::SQLite3)
vcmi_set_output_dir(gymserverplugin "")
enable_pch(gymserverplugin)
