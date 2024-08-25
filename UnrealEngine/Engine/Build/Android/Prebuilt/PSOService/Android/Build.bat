@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%
set PATH_TO_CMAKE_BIN=%ANDROID_HOME%/cmake/3.22.1/bin

REM Temporary build directories (used as working directories when running CMake)
set ANDROID_BUILD_PATH="%PATH_TO_CMAKE_FILE%\Build"
set MAKE="%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe"
set CMAKE_TOOLCHAIN_FILE="%NDKROOT%\build\cmake\android.toolchain.cmake"
rem ..\..\CMake\PlatformScripts\Android\Android.cmake"
set ENGINE_ROOT="%PATH_TO_CMAKE_FILE%\..\..\..\..\..\"

set BUILD_TYPE=Release
set BUILD_ARCH=arm64-v8a
set BUILD_ARCH_PATH=arm64-v8a
set NATIVE_API_LEVEL=android-26
call :Build
set BUILD_ARCH=x86_64
set BUILD_ARCH_PATH=x86_64
set NATIVE_API_LEVEL=android-26
call :Build

PAUSE
goto Exit

:Exit
endlocal
GOTO:EOF

:Build

if exist "%ANDROID_BUILD_PATH%" (rmdir "%ANDROID_BUILD_PATH%" /s/q)

echo Building for Android (%BUILD_ARCH%) %BUILD_TYPE%, API: %NATIVE_API_LEVEL%
mkdir "%ANDROID_BUILD_PATH%"
pushd "%ANDROID_BUILD_PATH%" 
%PATH_TO_CMAKE_BIN%/cmake.exe -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE% -G "MinGW Makefiles" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM=%MAKE% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DANDROID_NATIVE_API_LEVEL=%NATIVE_API_LEVEL% DANDROID_PLATFORM=%NATIVE_API_LEVEL% -DANDROID_ABI=%BUILD_ARCH% -DANDROID_STL=c++_static %PATH_TO_CMAKE_FILE%

REM Now compile it
%MAKE% -j %NUMBER_OF_PROCESSORS%
mkdir %PATH_TO_CMAKE_FILE%\%BUILD_TYPE%\%BUILD_ARCH_PATH%
move /y %ANDROID_BUILD_PATH%\libpsoservice.so %PATH_TO_CMAKE_FILE%\%BUILD_TYPE%\%BUILD_ARCH_PATH%
REM move /y %ANDROID_BUILD_PATH%\libunwindbacktrace.a %PATH_TO_CMAKE_FILE%\%BUILD_TYPE%\%BUILD_ARCH_PATH%
popd
rmdir "%ANDROID_BUILD_PATH%" /s/q
