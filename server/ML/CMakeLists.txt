cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

find_package(SQLite3 REQUIRED)

add_library(mlserverplugin SHARED
  Config.h
  ServerPlugin.cpp
  ServerPlugin.h
  Stats.cpp
  Stats.h

  StdInc.h
)

add_dependencies(mlserverplugin vcmi)
target_link_libraries(mlserverplugin PRIVATE vcmi SQLite::SQLite3)
vcmi_set_output_dir(mlserverplugin "")
enable_pch(mlserverplugin)
