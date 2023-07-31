#include(../../CMake/PlatformScripts/Android/Android.cmake)
include($ENV{NDKROOT}/build/cmake/android.toolchain.cmake)

set(CMAKE_CXX_FLAGS_DEBUG "-O0" CACHE TYPE INTERNAL FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE TYPE INTERNAL FORCE)

add_definitions(-DHAVE_EXPAT_CONFIG_H) # -DWIN32)
