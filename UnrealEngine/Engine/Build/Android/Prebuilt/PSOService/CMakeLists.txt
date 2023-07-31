# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

message(STATUS "Using NDK: ${CMAKE_ANDROID_NDK}")

add_compile_options(-g -Wall -Wextra -Werror -Wno-unused-parameter)

SET(UE4LOCATION "${BUILDDIR}/../../../../")

link_directories(
        ${BUILDDIR}/libs/arm64-v8a
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

find_library(
        android-lib
        android
)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}

        ${android-lib})

