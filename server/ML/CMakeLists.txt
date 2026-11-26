cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# Conan target is "sqlite3", linux apt target is "SQLite3"
find_package(sqlite3 CONFIG QUIET)

if(TARGET sqlite3::sqlite3)
    set(SQLITE3_TARGET sqlite3::sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    set(SQLITE3_TARGET SQLite::SQLite3)
endif()

add_library(mlserverplugin_objects OBJECT
  BattleCheckpoint.cpp
  Config.h
  ServerPlugin.cpp
  ServerPlugin.h
  Stats.cpp
  Stats.h

  StdInc.h
)

set_target_properties(mlserverplugin_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(mlserverplugin_objects PRIVATE vcmi ${SQLITE3_TARGET})
enable_pch(mlserverplugin_objects)

add_library(mlserverplugin SHARED $<TARGET_OBJECTS:mlserverplugin_objects>)
target_link_libraries(mlserverplugin PRIVATE vcmi ${SQLITE3_TARGET})
vcmi_set_output_dir(mlserverplugin "")

if(ENABLE_TEST)
    # vcmitest links the core objects, so the plugin must use the same library globals.
    add_library(mlserverplugin_test STATIC $<TARGET_OBJECTS:mlserverplugin_objects>)
    target_link_libraries(mlserverplugin_test PRIVATE ${SQLITE3_TARGET})
endif()
