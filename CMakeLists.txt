cmake_minimum_required(VERSION 3.8)
project(crystalize)

set(
  SRCS
  src/crystalize.h
  src/crystalize.c
)

# TODO: max out the warning settings for the compilers (why isn't there a generic way to do this?)
#if(MSVC)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
#endif()
if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Werror")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Werror")
endif()


add_library(crystalize STATIC ${SRCS})
target_compile_features(crystalize PRIVATE cxx_std_11)
target_compile_features(crystalize PUBLIC cxx_variadic_macros)
target_include_directories(
  crystalize
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:>
)

set(
  TEST_SRCS
  spec/catch.hpp
  spec/main.cpp
  spec/schema_spec.cpp
)

add_executable(test_runner ${TEST_SRCS})
target_compile_features(test_runner PRIVATE cxx_std_11)
target_link_libraries(test_runner crystalize)

enable_testing()
add_test(NAME spec COMMAND test_runner)
