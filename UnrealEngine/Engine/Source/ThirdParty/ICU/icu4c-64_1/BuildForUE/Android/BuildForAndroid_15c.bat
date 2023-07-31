@ECHO OFF

setlocal

set ROOT_DIR=%CD%
set OUTPUT_ROOT_DIR=%CD%/../../lib/Android

REM NDK OVERRIDE. Use a different NDK then your environment variable NDKROOT by setting it here
set NDKROOT=g:/android-ndk-r15c

set PATH=%PATH%;g:/cmake-3.14.5-win64-x64/bin

set PATH_TO_CMAKE_FILE=%ROOT_DIR%/../
rem set PATH_TO_CROSS_COMPILE_CMAKE_FILES=%ROOT_DIR%/../../../../android/cmake/android.toolchain.cmake
set PATH_TO_CROSS_COMPILE_CMAKE_FILES=%NDKROOT%/build/cmake/android.toolchain.cmake
set ANDROID_BUILD_PATH_ROOT=%ROOT_DIR%/Build

set ANDROID_NMAKE_EXEC=%NDKROOT%/prebuilt/windows-x86_64/bin/make.exe

REM Remove Build folder and recreate it to clear it out
if exist "%ANDROID_BUILD_PATH_ROOT%" (rmdir "%ANDROID_BUILD_PATH_ROOT%" /s/q)
mkdir "%ANDROID_BUILD_PATH_ROOT%"

REM Set any additional flags here
set CXXFLAGS="-std=c++14"
set AdditionalCompileArguments=-DISANDROID=1

set OutputFileName=libicu
set OutputDebugFileAddition=
set OutputFileExtension=.a

rem ----------------------------------------------------------------------------------
rem --                            ARMv7(Debug)                                      --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for ARMv7(Debug)
set AndroidABI=armeabi-v7a
set AndroidAPILevel=android-19
set BuildType=Debug
set DestFolder=%OUTPUT_ROOT_DIR%/ARMv7/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            ARMv7(Release)                                    --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for ARMv7(Release)
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%/ARMv7/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            ARM64(Debug)                                      --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for ARM64(Debug)
set AndroidABI=arm64-v8a
set AndroidAPILevel=android-21
set BuildType=Debug
set DestFolder=%OUTPUT_ROOT_DIR%/ARM64/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            ARM64(Release)                                    --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for ARM64(Release)
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%/ARM64/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            x86(Debug)                                        --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for x86(Debug)
set AndroidABI=x86
set AndroidAPILevel=android-19
set BuildType=Debug
set DestFolder=%OUTPUT_ROOT_DIR%/x86/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            x86(Release)                                      --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for x86(Release)
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%/x86/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            x64(Debug)                                        --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for x64(Debug)
set AndroidABI=x86_64
set AndroidAPILevel=android-21
set BuildType=Debug
set DestFolder=%OUTPUT_ROOT_DIR%/x64/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputDebugFileAddition%%OutputFileExtension%"


rem ----------------------------------------------------------------------------------
rem --                            x64(Release)                                      --
rem ----------------------------------------------------------------------------------
echo Building FreeType makefile for x64(Release)
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%/x64/%BuildType%

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="gnustl_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

move /y "%cd%/../%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"


