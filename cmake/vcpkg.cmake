include(FetchContent)

option(USE_VCPKG "Use vcpkg to provide dependencies" ON)

if(${USE_VCPKG})
	FetchContent_Declare(
		vcpkg
		GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
		GIT_TAG master
		GIT_SHALLOW true
		PATCH_COMMAND cmake -E rm -f "$<IF:${CMAKE_HOST_WIN32},vcpkg.exe,vcpkg>"
	)

	FetchContent_MakeAvailable(vcpkg)
	include(${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake)
endif()
