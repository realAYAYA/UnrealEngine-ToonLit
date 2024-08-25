project(libpsoservice)

cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DHAVE_CONFIG_H -D_GNU_SOURCE -DANDROID_PLATFORM=android-26)

if(CMAKE_BUILD_TYPE EQUAL "RELEASE")
	add_definitions(-DNDEBUG)
endif()

set(SOURCE_ROOT
    ${CMAKE_SOURCE_DIR}/../Source
)

set(libpsoservice_includes
    ${SOURCE_ROOT}/src
    ${CMAKE_SOURCE_DIR}/../../../../../Source/ThirdParty/Vulkan/Include
)

include_directories(${libpsoservice_includes}) 
set(libpsoservice_source
    ${SOURCE_ROOT}/PSOService.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -std=c++11 \
                     -DVK_USE_PLATFORM_ANDROID_KHR")

add_library(psoservice SHARED ${libpsoservice_source})
find_library(log log)
find_library(log-lib log )
find_library(android-lib android)

		find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "${CMAKE_SOURCE_DIR}/libs/vulkan" REQUIRED)
		IF (Vulkan_LIBRARY)
			set(Vulkan_FOUND ON)
			MESSAGE("Using bundled Vulkan library version ${Vulkan_LIBRARY} - ${vulkan}")
			MESSAGE($(Vulkan_LIBRARY))
			MESSAGE($(vulkan))
		ENDIF()

target_link_libraries(psoservice ${Vulkan_LIBRARY} ${log-lib} ${android-lib})

# Enable support for non-4k virtual page sizes
target_link_options(psoservice PRIVATE -z max-page-size=65536)