# This file is based off of the Platform/Darwin.cmake and Platform/UnixPaths.cmake
# files which are included with CMake 2.8.4
# It has been altered for tvOS development

# Updated to XCode 7.x July 2016, Epic Games

# Options:
#
# TVOS_PLATFORM = OS (default) or SIMULATOR or SIMULATOR64
#   This decides if SDKS will be selected from the TVOS.platform or appleTVSimulator.platform folders
#   OS - the default, used to build for AppleTV physical devices, which have an arm arch.
#   SIMULATOR - used to build for the Simulator platforms, which have an x86 arch.
#
# CMAKE_TVOS_DEVELOPER_ROOT = automatic(default) or /path/to/platform/Developer folder
#   By default this location is automatcially chosen based on the TVOS_PLATFORM value above.
#   If set manually, it will override the default location and force the user of a particular Developer Platform
#
# CMAKE_TVOS_SDK_ROOT = automatic(default) or /path/to/platform/Developer/SDKs/SDK folder
#   By default this location is automatcially chosen based on the CMAKE_TVOS_DEVELOPER_ROOT value.
#   In this case it will always be the most up-to-date SDK found in the CMAKE_TVOS_DEVELOPER_ROOT path.
#   If set manually, this will force the use of a specific SDK version

# Macros:
#
# set_xcode_property (TARGET XCODE_PROPERTY XCODE_VALUE)
#  A convenience macro for setting xcode specific properties on targets
#  example: set_xcode_property (mytvoslib TVOS_DEPLOYMENT_TARGET "3.1")
#
# find_host_package (PROGRAM ARGS)
#  A macro used to find executable programs on the host system, not within the tvOS environment.
#  Thanks to the android-cmake project for providing the command

# Standard settings
#set (CMAKE_SYSTEM_NAME Darwin)
#set (CMAKE_SYSTEM_VERSION 1)
set (UNIX True)
set (APPLE True)
set (TVOS True)

# Required as of cmake 2.8.10
set (CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "Force unset of the deployment target for tvOS" FORCE)

# Determine the cmake host system version so we know where to find the tvOS SDKs
find_program (CMAKE_UNAME uname /bin /usr/bin /usr/local/bin)
if (CMAKE_UNAME)
	exec_program(uname ARGS -r OUTPUT_VARIABLE CMAKE_HOST_SYSTEM_VERSION)
	STRING (REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\1" DARWIN_MAJOR_VERSION "${CMAKE_HOST_SYSTEM_VERSION}")
endif (CMAKE_UNAME)

# Force the compilers to clang for tvOS
#include (CMakeForceCompiler)
#set (CMAKE_C_COMPILER /usr/bin/clang)
#set (CMAKE_CXX_COMPILER /usr/bin/clang++)
#set (CMAKE_AR "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar" CACHE FILEPATH "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar" FORCE)

# Use libtool instead of AR for the creation of achives
set(CMAKE_C_ARCHIVE_CREATE "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/libtool -static -syslibroot ${CMAKE_TVOS_SDK_ROOT} -o <TARGET> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_CREATE "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/libtool -static -syslibroot ${CMAKE_TVOS_SDK_ROOT} -o <TARGET> <OBJECTS>")

# Skip the platform compiler checks for cross compiling
set (CMAKE_CXX_COMPILER_WORKS TRUE)
set (CMAKE_C_COMPILER_WORKS TRUE)

# All tvOS/Darwin specific settings - some may be redundant
set (CMAKE_SHARED_LIBRARY_PREFIX "lib")
set (CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
set (CMAKE_SHARED_MODULE_PREFIX "lib")
set (CMAKE_SHARED_MODULE_SUFFIX ".so")
set (CMAKE_MODULE_EXISTS 1)
set (CMAKE_DL_LIBS "")

set (CMAKE_C_OSX_COMPATIBILITY_VERSION_FLAG "-compatibility_version ")
set (CMAKE_C_OSX_CURRENT_VERSION_FLAG "-current_version ")
set (CMAKE_CXX_OSX_COMPATIBILITY_VERSION_FLAG "${CMAKE_C_OSX_COMPATIBILITY_VERSION_FLAG}")
set (CMAKE_CXX_OSX_CURRENT_VERSION_FLAG "${CMAKE_C_OSX_CURRENT_VERSION_FLAG}")

# Hidden visibilty is required for cxx on tvOS 
add_definitions(-fvisibility=hidden -fvisibility-inlines-hidden)

add_definitions(-fembed-bitcode)

# Add in tvos min version flag
if (DEFINED CMAKE_TVOS_DEPLOYMENT_TARGET)
	add_definitions(-mtvos-version-min=${CMAKE_TVOS_DEPLOYMENT_TARGET})
endif (DEFINED CMAKE_TVOS_DEPLOYMENT_TARGET)

set (CMAKE_C_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_C_LINK_FLAGS}")
set (CMAKE_CXX_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_CXX_LINK_FLAGS}")

set (CMAKE_PLATFORM_HAS_INSTALLNAME 1)
set (CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-dynamiclib -headerpad_max_install_names")
set (CMAKE_SHARED_MODULE_CREATE_C_FLAGS "-bundle -headerpad_max_install_names")
set (CMAKE_SHARED_MODULE_LOADER_C_FLAG "-Wl,-bundle_loader,")
set (CMAKE_SHARED_MODULE_LOADER_CXX_FLAG "-Wl,-bundle_loader,")
set (CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".so" ".a")

# hack: if a new cmake (which uses CMAKE_INSTALL_NAME_TOOL) runs on an old build tree
# (where install_name_tool was hardcoded) and where CMAKE_INSTALL_NAME_TOOL isn't in the cache
# and still cmake didn't fail in CMakeFindBinUtils.cmake (because it isn't rerun)
# hardcode CMAKE_INSTALL_NAME_TOOL here to install_name_tool, so it behaves as it did before, Alex
if (NOT DEFINED CMAKE_INSTALL_NAME_TOOL)
	find_program(CMAKE_INSTALL_NAME_TOOL install_name_tool)
endif (NOT DEFINED CMAKE_INSTALL_NAME_TOOL)

# Setup tvOS platform unless specified manually with TVOS_PLATFORM
if (NOT DEFINED TVOS_PLATFORM)
	set (TVOS_PLATFORM "OS")
endif (NOT DEFINED TVOS_PLATFORM)
set (TVOS_PLATFORM ${TVOS_PLATFORM} CACHE STRING "Type of tvOS Platform")

# Setup building for arm64 or not
#if (NOT DEFINED BUILD_ARM64)
#    set (BUILD_ARM64 true)
#endif (NOT DEFINED BUILD_ARM64)
#set (BUILD_ARM64 ${BUILD_ARM64} CACHE STRING "Build arm64 arch or not")

# Check the platform selection and setup for developer root
if (${TVOS_PLATFORM} STREQUAL "OS")
	set (TVOS_PLATFORM_LOCATION "tvOSOS.platform")

	# This causes the installers to properly locate the output libraries
	set (CMAKE_XCODE_EFFECTIVE_PLATFORMS "-tvos")
elseif (${TVOS_PLATFORM} STREQUAL "SIMULATOR")
    set (SIMULATOR true)
	set (TVOS_PLATFORM_LOCATION "appleTVSimulator.platform")

	# This causes the installers to properly locate the output libraries
	set (CMAKE_XCODE_EFFECTIVE_PLATFORMS "-appletvsimulator")
elseif (${TVOS_PLATFORM} STREQUAL "SIMULATOR64")
    set (SIMULATOR true)
	set (TVOS_PLATFORM_LOCATION "appleTVSimulator.platform")

	# This causes the installers to properly locate the output libraries
	set (CMAKE_XCODE_EFFECTIVE_PLATFORMS "-appletvsimulator")
else (${TVOS_PLATFORM} STREQUAL "OS")
	message (FATAL_ERROR "Unsupported TVOS_PLATFORM value selected. Please choose OS or SIMULATOR")
endif (${TVOS_PLATFORM} STREQUAL "OS")

# Setup tvOS developer location unless specified manually with CMAKE_TVOS_DEVELOPER_ROOT
# Note Xcode 4.3 changed the installation location, choose the most recent one available
exec_program(/usr/bin/xcode-select ARGS -print-path OUTPUT_VARIABLE CMAKE_XCODE_DEVELOPER_DIR)
set (XCODE_POST_43_ROOT "${CMAKE_XCODE_DEVELOPER_DIR}/Platforms/${TVOS_PLATFORM_LOCATION}/Developer")
set (XCODE_PRE_43_ROOT "/Developer/Platforms/${TVOS_PLATFORM_LOCATION}/Developer")
if (NOT DEFINED CMAKE_TVOS_DEVELOPER_ROOT)
	if (EXISTS ${XCODE_POST_43_ROOT})
		set (CMAKE_TVOS_DEVELOPER_ROOT ${XCODE_POST_43_ROOT})
	elseif(EXISTS ${XCODE_PRE_43_ROOT})
		set (CMAKE_TVOS_DEVELOPER_ROOT ${XCODE_PRE_43_ROOT})
	endif (EXISTS ${XCODE_POST_43_ROOT})
endif (NOT DEFINED CMAKE_TVOS_DEVELOPER_ROOT)
set (CMAKE_TVOS_DEVELOPER_ROOT ${CMAKE_TVOS_DEVELOPER_ROOT} CACHE PATH "Location of tvOS Platform")

# Find and use the most recent tvOS sdk unless specified manually with CMAKE_TVOS_SDK_ROOT
if (NOT DEFINED CMAKE_TVOS_SDK_ROOT)
	file (GLOB _CMAKE_TVOS_SDKS "${CMAKE_TVOS_DEVELOPER_ROOT}/SDKs/*")
	if (_CMAKE_TVOS_SDKS) 
		list (SORT _CMAKE_TVOS_SDKS)
		list (REVERSE _CMAKE_TVOS_SDKS)
		list (GET _CMAKE_TVOS_SDKS 0 CMAKE_TVOS_SDK_ROOT)
	else (_CMAKE_TVOS_SDKS)
		message (FATAL_ERROR "No tvOS SDK's found in default search path ${CMAKE_TVOS_DEVELOPER_ROOT}. Manually set CMAKE_TVOS_SDK_ROOT or install the tvOS SDK.")
	endif (_CMAKE_TVOS_SDKS)
	message (STATUS "Toolchain using default tvOS SDK: ${CMAKE_TVOS_SDK_ROOT}")
endif (NOT DEFINED CMAKE_TVOS_SDK_ROOT)
set (CMAKE_TVOS_SDK_ROOT ${CMAKE_TVOS_SDK_ROOT} CACHE PATH "Location of the selected tvOS SDK")

# Set the sysroot default to the most recent SDK
set (CMAKE_OSX_SYSROOT ${CMAKE_TVOS_SDK_ROOT} CACHE PATH "Sysroot used for tvOS support")

# set the architecture for tvOS 
if (${TVOS_PLATFORM} STREQUAL "OS")
    set (TVOS_ARCH armv7 armv7s arm64)
elseif (${TVOS_PLATFORM} STREQUAL "SIMULATOR")
    set (TVOS_ARCH i386)
elseif (${TVOS_PLATFORM} STREQUAL "SIMULATOR64")
    set (TVOS_ARCH x86_64)
endif (${TVOS_PLATFORM} STREQUAL "OS")

set (CMAKE_OSX_ARCHITECTURES ${TVOS_ARCH} CACHE STRING  "Build architecture for tvOS")

# Set the find root to the tvOS developer roots and to user defined paths
set (CMAKE_FIND_ROOT_PATH ${CMAKE_TVOS_DEVELOPER_ROOT} ${CMAKE_TVOS_SDK_ROOT} ${CMAKE_PREFIX_PATH} CACHE STRING  "tvOS find search path root")

# default to searching for frameworks first
set (CMAKE_FIND_FRAMEWORK FIRST)

# set up the default search directories for frameworks
set (CMAKE_SYSTEM_FRAMEWORK_PATH
	${CMAKE_TVOS_SDK_ROOT}/System/Library/Frameworks
	${CMAKE_TVOS_SDK_ROOT}/System/Library/PrivateFrameworks
	${CMAKE_TVOS_SDK_ROOT}/Developer/Library/Frameworks
)

# only search the tvOS sdks, not the remainder of the host filesystem
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)


# This little macro lets you set any XCode specific property
macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
	set_property (TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY} ${XCODE_VALUE})
endmacro (set_xcode_property)


# This macro lets you find executable programs on the host system
macro (find_host_package)
	set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
	set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
	set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
	set (TVOS FALSE)

	find_package(${ARGN})

	set (TVOS TRUE)
	set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
	set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
	set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endmacro (find_host_package)

