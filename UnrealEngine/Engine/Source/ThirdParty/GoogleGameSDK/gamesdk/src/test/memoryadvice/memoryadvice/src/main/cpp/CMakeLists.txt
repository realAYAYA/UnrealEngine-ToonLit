cmake_minimum_required(VERSION 3.4.1)

add_library(
        try-alloc-lib
        SHARED
        try-alloc.cc)

target_link_libraries(
        try-alloc-lib)
