cmake_minimum_required(VERSION 3.8)
project(vgen VERSION 1.0.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

option(WARN_AS_ERROR "Treat warnings as errors")

find_package(pugixml REQUIRED)
find_package(fmt REQUIRED)
find_path(CXXOPTS_INCLUDE_DIR "cxxopts.hpp")

add_executable(vgen src/main.cpp)
target_compile_features(vgen PUBLIC cxx_std_17)
target_include_directories(vgen PRIVATE ${CXXOPTS_INCLUDE_DIR})
target_link_libraries(vgen PRIVATE pugixml)
target_link_libraries(vgen PRIVATE fmt::fmt-header-only)

if (MSVC)
	# multi-process build
	target_compile_options(vgen PUBLIC /MP)

	# enable extra warnings
	target_compile_options(vgen PUBLIC /W4)

	# treat warnings as errors
	if (WARN_AS_ERROR)
		target_compile_options(vgen PUBLIC /WX)

		# suppress warning in pugixml: non dll-interface class 'std::exception' used as base for dll-interface class 'pugi::xpath_exception'
		target_compile_options(vgen PUBLIC /wd4275)

		# suppress deprecation warnings for std::allocator in fmt 4.0.0
		target_compile_definitions(vgen PUBLIC _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
	endif()
else()
	# enable extra warnings
	target_compile_options(vgen PUBLIC -Wall -Wextra -Wpedantic)

	# treat warnings as errors
	if (WARN_AS_ERROR)
		target_compile_options(vgen PUBLIC -Werror)
	endif()
endif()
