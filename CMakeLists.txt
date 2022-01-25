cmake_minimum_required(VERSION 3.15)
project(vgen VERSION 2.0.1)

include(cmake/PreventInSourceBuilds.cmake)
include(cmake/StandardProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
set_project_warnings(project_options)

add_subdirectory(src)

option(BUILD_TESTS "Build the test" ON)

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
