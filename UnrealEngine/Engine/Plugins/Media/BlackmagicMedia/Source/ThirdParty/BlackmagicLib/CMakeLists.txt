cmake_minimum_required(VERSION 3.0.0)
project(UE4BlackmagicDesign VERSION 0.1.0)

include(CTest)
enable_testing()

add_library(
	BlackmagicLib SHARED
    Include/Linux/DeckLinkAPIDispatch.cpp
)

# target_precompile_headers(BlackmagicLib PRIVATE Source/stdafx.h)
target_include_directories(
	BlackmagicLib PRIVATE
    ../../../../../../../Source/ThirdParty/Unix/LibCxx/include
    ../../../../../../../Source/ThirdParty/Unix/LibCxx/include/c++/v1
    .
    Include
)

target_link_directories(
	BlackmagicLib PRIVATE
    ../../../../../../../Source/ThirdParty/Unix/LibCxx/lib/Linux/x86_64-unknown-linux-gnu
)

set_target_properties(
    BlackmagicLib PROPERTIES
    CXX_STANDARD 17
    DEFINE_SYMBOL UEBLACKMAGICDESIGN_EXPORTS
    ARCHIVE_OUTPUT_DIRECTORY "../../../../../../../Plugins/Media/BlackmagicMedia/Binaries/ThirdParty/Linux"
    LIBRARY_OUTPUT_DIRECTORY "../../../../../../../Plugins/Media/BlackmagicMedia/Binaries/ThirdParty/Linux"
    RUNTIME_OUTPUT_DIRECTORY "../../../../../../../Plugins/Media/BlackmagicMedia/Binaries/ThirdParty/Linux"
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

#add_compile_definitions(BMD_PUBLIC=__attribute__\(\(visibility\(\"default\"\)\)\))
    
include(CPack)
