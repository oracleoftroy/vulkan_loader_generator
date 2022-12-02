include(FetchContent)

option(USE_VCPKG "Use vcpkg to provide dependencies" ON)

set(HOST_IS_WINDOWS $<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>)
set(VCPKG_EXE_NAME $<IF:${HOST_IS_WINDOWS},vcpkg.exe,vcpkg>)

if(${USE_VCPKG})
	FetchContent_Declare(
		vcpkg
		GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
		GIT_TAG master
		GIT_SHALLOW true
		PATCH_COMMAND cmake -E rm -f ${VCPKG_EXE_NAME}
	)

	FetchContent_MakeAvailable(vcpkg)
	include(${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake)
endif()
