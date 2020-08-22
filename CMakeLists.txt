cmake_minimum_required(VERSION 3.8)
project(vgen VERSION 1.0.0)

include(cmake/PreventInSourceBuilds.cmake)
include(cmake/StandardProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
set_project_warnings(project_options)

add_subdirectory(src)

if(BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()
