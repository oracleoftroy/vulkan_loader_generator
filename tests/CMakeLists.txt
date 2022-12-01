add_executable(vgen-tests "vgen-parser.tests.cpp" "vgen-output.tests.cpp")
find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(vgen-tests PRIVATE project_options vgen-lib Catch2::Catch2WithMain)

include(CTest)
include(Catch)

catch_discover_tests(
	vgen-tests
	TEST_PREFIX "unittests."
	EXTRA_ARGS -s --reporter=xml --out=tests.xml
)
