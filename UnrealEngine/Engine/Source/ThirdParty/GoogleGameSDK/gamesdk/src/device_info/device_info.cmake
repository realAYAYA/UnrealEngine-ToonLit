cmake_minimum_required(VERSION 3.4.1)
set( _MY_DIR ${CMAKE_CURRENT_LIST_DIR})

include("${_MY_DIR}/../protobuf/protobuf.cmake")

protobuf_generate_nano_c( ${_MY_DIR}/../../include/device_info
  ${_MY_DIR}/../../include/device_info/device_info.proto)

set ( SOURCE_LOCATION "${_MY_DIR}")
set ( THIRD_PARTY_LOCATION "${_MY_DIR}/../../third_party")

include_directories( ${_MY_DIR}/../../include )
include_directories( ${_MY_DIR}/../../include/third_party/nanopb )
include_directories( ${THIRD_PARTY_LOCATION}/stream_util/include )
include_directories( ${_MY_DIR}/../common )

include_directories( ${PROTO_GENS_DIR} )
include_directories( ${PROTOBUF_INCLUDE_DIR} )

message( STATUS "Building device_info_static to ${CMAKE_CURRENT_BINARY_DIR}/build" )
file(GLOB Texture_test_cases_SOURCES ${SOURCE_LOCATION}/core/texture_test_cases/*.c)
add_library( device_info_static

             STATIC

             ${THIRD_PARTY_LOCATION}/stream_util/src/getdelim.cpp
             ${SOURCE_LOCATION}/core/basic_texture_renderer.cpp
             ${SOURCE_LOCATION}/core/device_info.cpp
             ${SOURCE_LOCATION}/core/texture_test_cases.cpp
             ${Texture_test_cases_SOURCES}
             ${PROTO_GENS_DIR}/nano/device_info.pb.c
             ${PROTOBUF_NANO_SRCS}
             )

set_target_properties( device_info_static PROPERTIES
     LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/build )
target_compile_definitions(device_info_static PRIVATE -DPB_FIELD_16BIT=1)

add_library( game_sdk_device_info_jni

             SHARED

             ${SOURCE_LOCATION}/jni/device_info_jni.cpp
             )


target_link_libraries( game_sdk_device_info_jni

                    device_info_static
                    android
                    EGL
                    GLESv2
                    log )
extra_pb_nano_link_options(game_sdk_device_info_jni)
