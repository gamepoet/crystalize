cmake_minimum_required(VERSION 3.8)
project(crystalize)

set(
  SRCS
  src/config.c
  src/config.h
  src/encoder_decode.c
  src/encoder_encode.c
  src/encoder.h
  src/crystalize.c
  src/crystalize.h
  src/hash.c
  src/hash.h
  src/writer.c
  src/writer.h
)

# max out the warning settings for the compilers (why isn't there a generic way to do this?)
if(MSVC)
  string(REPLACE "/W3" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  string(REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
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
if(MSVC)
  target_compile_options(crystalize PRIVATE /W4 /WX /wd4100)
elseif(APPLE)
  target_compile_options(crystalize PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-missing-braces -Wno-missing-field-initializers)
endif()

set(
  TEST_SRCS
  spec/catch.hpp
  spec/main.cpp
  spec/schema_spec.cpp
)

add_executable(test_runner ${TEST_SRCS})
target_compile_features(test_runner PRIVATE cxx_std_11)
target_link_libraries(test_runner crystalize)
if(MSVC)
  target_compile_options(test_runner PRIVATE /W4 /WX /wd4100)
elseif(APPLE)
  target_compile_options(test_runner PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-missing-braces -Wno-missing-field-initializers)
endif()

enable_testing()
add_test(NAME spec COMMAND test_runner)
