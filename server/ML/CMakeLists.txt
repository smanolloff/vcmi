cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# Conan target is "sqlite3", linux apt target is "SQLite3"
find_package(sqlite3 CONFIG QUIET)

if(TARGET sqlite3::sqlite3)
    set(SQLITE3_TARGET sqlite3::sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    set(SQLITE3_TARGET SQLite::SQLite3)
endif()

add_library(mlserverplugin SHARED
  Config.h
  ServerPlugin.cpp
  ServerPlugin.h
  Stats.cpp
  Stats.h

  StdInc.h
)

add_dependencies(mlserverplugin vcmi)
target_link_libraries(mlserverplugin PRIVATE vcmi ${SQLITE3_TARGET})
vcmi_set_output_dir(mlserverplugin "")
enable_pch(mlserverplugin)
