cmake_minimum_required(VERSION 3.18.1)
project(swappy C CXX)
set(CMAKE_CXX_STANDARD 14)
set(IgnoreOldToolchainWarning "${ANDROID_UNIFIED_HEADERS}")

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wthread-safety" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D _LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS -O3 -fPIC" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0")
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D ENABLE_SWAPPY_LOGGING=1")
if ( DEFINED GAMESDK_THREAD_CHECKS )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGAMESDK_THREAD_CHECKS=${GAMESDK_THREAD_CHECKS}" )
endif()

set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections" )
set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-s" )
set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--hash-style=both" )

set ( SOURCE_LOCATION .)
set ( SOURCE_LOCATION_COMMON "${SOURCE_LOCATION}/common" )
set ( SOURCE_LOCATION_OPENGL "${SOURCE_LOCATION}/opengl" )
set ( SOURCE_LOCATION_VULKAN "${SOURCE_LOCATION}/vulkan" )

include_directories( ../include )
include_directories( ../src/common )
include_directories( common )
include_directories( opengl )
include_directories( vulkan )
include_directories( ${ANDROID_NDK}/sources/third_party/vulkan/src/common )

message( STATUS "Building swappy_static to ${CMAKE_CURRENT_BINARY_DIR}/build" )


# In order to support ChoreographerCallbacks back on older android versions we have a hack here
# which adds the dex file from :games-frame-pacing:extras to our binary.
# Android studio seems to use two different linkers and only one of those linkers requires we pass
# in the emulation option "-m".
# This linker which will be found in a path similar to this:
# 21.4.7075529/toolchains/llvm/prebuilt/linux-x86_64/bin/i686-linux-android-ld
# does not require the "-m" option.
# The more general linker which can be found in a path similar to this:
# 21.4.7075529/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld
# does require the "-m" option
if(CMAKE_LINKER MATCHES ".*/ld.lld" AND ANDROID_NDK_MAJOR GREATER 17)
  if ( ANDROID_NDK_ABI_NAME MATCHES "armeabi-v7a")
    set (LINKER_TARGET_EMULATION_OPTION "-m" "armelf_linux_eabi")
  elseif(ANDROID_NDK_ABI_NAME MATCHES "arm64-v8a")
    set (LINKER_TARGET_EMULATION_OPTION "-m" "aarch64linux")
  elseif(ANDROID_NDK_ABI_NAME MATCHES "x86_64")
    set (LINKER_TARGET_EMULATION_OPTION "-m" "elf_x86_64")
  elseif(ANDROID_NDK_ABI_NAME MATCHES "x86")
      set (LINKER_TARGET_EMULATION_OPTION "-m" "elf_i386")
  endif()
endif()

add_custom_command(OUTPUT classes_dex.o
     COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/extras/out/intermediates/dex/release/mergeDexRelease
     && ${CMAKE_LINKER}  ${LINKER_TARGET_EMULATION_OPTION} -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/classes_dex.o classes.dex
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
     )

set_source_files_properties(
     classes_dex.o
     PROPERTIES
     EXTERNAL_OBJECT true
     GENERATED true
)

add_library( swappy_static

             STATIC

             ${SOURCE_LOCATION_COMMON}/ChoreographerFilter.cpp
             ${SOURCE_LOCATION_COMMON}/ChoreographerThread.cpp
             ${SOURCE_LOCATION_COMMON}/CpuInfo.cpp
             ${SOURCE_LOCATION_COMMON}/Settings.cpp
             ${SOURCE_LOCATION_COMMON}/Thread.cpp
             ${SOURCE_LOCATION_COMMON}/SwappyCommon.cpp
             ${SOURCE_LOCATION_COMMON}/swappy_c.cpp
             ${SOURCE_LOCATION_COMMON}/SwappyDisplayManager.cpp
             ${SOURCE_LOCATION_COMMON}/CPUTracer.cpp
	     ${SOURCE_LOCATION_COMMON}/FrameStatistics.cpp
             ${SOURCE_LOCATION_OPENGL}/EGL.cpp
             ${SOURCE_LOCATION_OPENGL}/swappyGL_c.cpp
             ${SOURCE_LOCATION_OPENGL}/SwappyGL.cpp
             ${SOURCE_LOCATION_OPENGL}/FrameStatisticsGL.cpp
             ${SOURCE_LOCATION_VULKAN}/swappyVk_c.cpp
             ${SOURCE_LOCATION_VULKAN}/SwappyVk.cpp
             ${SOURCE_LOCATION_VULKAN}/SwappyVkBase.cpp
             ${SOURCE_LOCATION_VULKAN}/SwappyVkFallback.cpp
             ${SOURCE_LOCATION_VULKAN}/SwappyVkGoogleDisplayTiming.cpp
             ${SOURCE_LOCATION}/../src/common/system_utils.cpp
             ${CMAKE_CURRENT_BINARY_DIR}/classes_dex.o
             # Add new source files here
             )

set_target_properties( swappy_static PROPERTIES
     LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/build )

add_library( swappy

             SHARED

             ${SOURCE_LOCATION_COMMON}/swappy_c.cpp
             ${SOURCE_LOCATION_OPENGL}/swappyGL_c.cpp
             ${SOURCE_LOCATION_VULKAN}/swappyVk_c.cpp)


target_link_libraries( swappy

                        swappy_static
                        android
                        GLESv2
                        log
			atomic)
