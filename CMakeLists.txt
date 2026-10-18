cmake_minimum_required(VERSION 3.14)
project(spent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(spent_lib src/core.cpp)
target_include_directories(spent_lib PUBLIC include)
if(MSVC)
  target_compile_options(spent_lib PRIVATE /W4)
else()
  target_compile_options(spent_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(spent src/main.cpp)
target_link_libraries(spent PRIVATE spent_lib)

enable_testing()
add_executable(spent_tests tests/test_core.cpp)
target_link_libraries(spent_tests PRIVATE spent_lib)
add_test(NAME spent_tests COMMAND spent_tests)

install(TARGETS spent RUNTIME DESTINATION bin)
