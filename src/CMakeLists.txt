add_library(vgen-lib STATIC "vgen.hpp" "vgen.cpp")
target_link_libraries(vgen-lib PRIVATE project_options)
target_include_directories(vgen-lib PUBLIC .)

find_package(pugixml CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(vgen-lib PUBLIC pugixml fmt::fmt-header-only)

add_executable(vgen "main.cpp")
target_link_libraries(vgen PRIVATE project_options vgen-lib)

find_package(cxxopts CONFIG REQUIRED)

target_link_libraries(vgen PRIVATE cxxopts::cxxopts)
