@ECHO OFF

setlocal

set LIBPNG_PATH=libPNG-1.6.37

REM create directories for libraries
mkdir %LIBPNG_PATH%\lib\Android\ARMv7
mkdir %LIBPNG_PATH%\lib\Android\ARM64
mkdir %LIBPNG_PATH%\lib\Android\x86
mkdir %LIBPNG_PATH%\lib\Android\x64

set ROOT_DIR=%CD%
set OUTPUT_ROOT_DIR=%CD%/%LIBPNG_PATH%/lib/Android

REM NDK OVERRIDE. Use a different NDK then your environment variable NDKROOT by setting it here
REM set NDKROOT=H:/ndk/

set PATH=%ANDROID_HOME%\cmake\3.10.2.4988404\bin;%PATH%

set PATH_TO_CMAKE_FILE=%ROOT_DIR%/%LIBPNG_PATH%
set PATH_TO_CROSS_COMPILE_CMAKE_FILES=%NDKROOT%/build/cmake/android.toolchain.cmake
set ANDROID_BUILD_PATH_ROOT=%ROOT_DIR%/Build

set PATH_TO_CROSS_COMPILE_CMAKE_FILES=%NDKROOT%/build/cmake/android.toolchain.cmake
set ANDROID_NMAKE_EXEC=%NDKROOT%/prebuilt/windows-x86_64/bin/make.exe

REM Remove Build folder and recreate it to clear it out
if exist "%ANDROID_BUILD_PATH_ROOT%" (rmdir "%ANDROID_BUILD_PATH_ROOT%" /s/q)
mkdir "%ANDROID_BUILD_PATH_ROOT%"

REM Set any additional flags here
set CXXFLAGS="-std=c++14 -Wno-sign-compare -DU_DISABLE_VERSION_SUFFIX=0"
set AdditionalCompileArguments=

set OutputFileName=libpng
set OutputDebugFileAddition=
set OutputFileExtension=.a

rem ----------------------------------------------------------------------------------
rem --                            ARMv7(Release)                                    --
rem ----------------------------------------------------------------------------------
echo Building LibPNG makefile for ARMv7(Release)
set AndroidABI=armeabi-v7a
set AndroidAPILevel=android-19
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%\ARMv7

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="c++_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

copy "%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%\%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"

rem ----------------------------------------------------------------------------------
rem --                            ARM64(Release)                                    --
rem ----------------------------------------------------------------------------------
echo Building LibPNG makefile for ARM64(Release)
set AndroidABI=arm64-v8a
set AndroidAPILevel=android-21
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%\ARM64

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="c++_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

copy "%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%\%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"

rem ----------------------------------------------------------------------------------
rem --                            x86(Release)                                      --
rem ----------------------------------------------------------------------------------
echo Building LibPNG makefile for x86(Release)
set AndroidABI=x86
set AndroidAPILevel=android-19
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%\x86

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="c++_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

copy "%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%\%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"

rem ----------------------------------------------------------------------------------
rem --                            x64(Release)                                      --
rem ----------------------------------------------------------------------------------
echo Building LibPNG makefile for x64(Release)
set AndroidABI=x86_64
set AndroidAPILevel=android-21
set BuildType=Release
set DestFolder=%OUTPUT_ROOT_DIR%\x64

if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
mkdir "%DestFolder%"

set CurrentAndroidBuildDir=%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%
mkdir "%CurrentAndroidBuildDir%"
cd %CurrentAndroidBuildDir%

cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CROSS_COMPILE_CMAKE_FILES%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%ANDROID_NMAKE_EXEC%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="c++_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %PATH_TO_CMAKE_FILE% 
cmake --build .

copy "%ANDROID_BUILD_PATH_ROOT%\%AndroidABI%\%BuildType%\%OutputFileName%%OutputFileExtension%" "%DestFolder%\%OutputFileName%%OutputFileExtension%"

