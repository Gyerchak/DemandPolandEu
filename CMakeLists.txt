cmake_minimum_required(VERSION 3.16)
project(DemandPolandEu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
  message(STATUS "Using nlohmann_json package")
else()
  message(STATUS "Using system nlohmann/json.hpp header")
endif()

add_executable(dpe
  src/main.cpp
  src/engine.cpp
  src/json_io.cpp
  src/server.cpp
)

target_include_directories(dpe PRIVATE src /usr/include)

if(nlohmann_json_FOUND)
  target_link_libraries(dpe PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_options(dpe PRIVATE -O2 -Wall -Wextra)

# Unit tests (simple assert-based binary; no extra framework)
enable_testing()
add_executable(dpe_tests
  src/engine.cpp
  tests/test_main.cpp
)
target_include_directories(dpe_tests PRIVATE src /usr/include)
if(nlohmann_json_FOUND)
  target_link_libraries(dpe_tests PRIVATE nlohmann_json::nlohmann_json)
endif()
target_compile_options(dpe_tests PRIVATE -O2 -Wall -Wextra)
add_test(NAME dpe_tests COMMAND dpe_tests)
