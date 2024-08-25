cmake_minimum_required(VERSION 3.22.1)
project(ScudoMemoryTrace)

message("Using NDK version ${CMAKE_ANDROID_NDK_VERSION}")

add_library(ScudoMemoryTrace SHARED
  ${CMAKE_SOURCE_DIR}/ScudoMemoryTrace.cpp
  ${CMAKE_SOURCE_DIR}/ScudoMemoryTrace.h
)

set_property(TARGET ScudoMemoryTrace PROPERTY CXX_STANDARD 20)

set_target_properties(ScudoMemoryTrace PROPERTIES COMPILE_FLAGS "-fno-exceptions -fno-rtti -fno-unwind-tables -g -O3")
set_target_properties(ScudoMemoryTrace PROPERTIES LINK_FLAGS_RELEASE "-s")
