cmake_minimum_required(VERSION 3.0)
project(hlslcc)

set (CMAKE_CXX_STANDARD 11)

if (MSVC)
	add_compile_options("/Zi")
else()
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

FILE(GLOB sources ../src/hlslcc_lib/*.cpp)

add_library(hlslcc STATIC ${sources})
