@echo OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

if not exist "%NDKROOT%" goto NDKRootMissing
if not exist "%UE4_EXPAT_ROOT_DIR%" goto ExpatMissing

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set ANDROID_BUILD_PATH="%PATH_TO_CMAKE_FILE%\..\Android\Build"
set ANDROID_LIB_PATH="%PATH_TO_CMAKE_FILE%\..\Android"
set MAKE="%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe"

echo Generating libstrophe makefile for Android (64-bit)...
if exist "%ANDROID_BUILD_PATH%" (rmdir "%ANDROID_BUILD_PATH%" /s/q)


@echo Building for Android Arm64 (64-bit) Release
mkdir "%ANDROID_BUILD_PATH%"
pushd "%ANDROID_BUILD_PATH%"
REM Generate Project Files
set UE4_OPENSSL_ARCH=arm64
cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\Android\Android.cmake" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM=%MAKE% -DCMAKE_BUILD_TYPE=Release -DANDROID_NATIVE_API_LEVEL=android-21 -DANDROID_ABI=arm64-v8a -DANDROID_STL=gnustl_shared -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%\\lib" %PATH_TO_CMAKE_FILE%
REM Now compile it
%MAKE%
if not exist "%ANDROID_LIB_PATH%\Release\arm64" mkdir "%ANDROID_LIB_PATH%\Release\arm64"
move /y %ANDROID_BUILD_PATH%\..\libstrophe.a %ANDROID_LIB_PATH%\Release\arm64
popd
rmdir "%ANDROID_BUILD_PATH%" /s/q

@echo Building for Android Arm64 (64-bit) Debug
mkdir "%ANDROID_BUILD_PATH%"
pushd "%ANDROID_BUILD_PATH%"
REM Generate Project Files
set UE4_OPENSSL_ARCH=arm64
cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\Android\Android.cmake" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM=%MAKE% -DCMAKE_BUILD_TYPE=Debug -DANDROID_NATIVE_API_LEVEL=android-21 -DANDROID_ABI=arm64-v8a -DANDROID_STL=gnustl_shared -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%\\lib" %PATH_TO_CMAKE_FILE%
REM Now compile it
%MAKE%
if not exist "%ANDROID_LIB_PATH%\Debug\arm64" mkdir "%ANDROID_LIB_PATH%\Debug\arm64"
move /y %ANDROID_BUILD_PATH%\..\libstrophe.a %ANDROID_LIB_PATH%\Debug\arm64
popd
rmdir "%ANDROID_BUILD_PATH%" /s/q


@echo Building for Android Arm7 (32-bit) Release
mkdir "%ANDROID_BUILD_PATH%"
pushd "%ANDROID_BUILD_PATH%"
REM Generate Project Files
set UE4_OPENSSL_ARCH=armv7
cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\Android\Android.cmake" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM=%MAKE% -DCMAKE_BUILD_TYPE=Release -DANDROID_NATIVE_API_LEVEL=android-19 -DANDROID_ABI=armeabi-v7a -DANDROID_STL=gnustl_shared -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%\\lib" %PATH_TO_CMAKE_FILE%
REM Now compile it
%MAKE%
if not exist "%ANDROID_LIB_PATH%\Release\armv7" mkdir "%ANDROID_LIB_PATH%\Release\armv7"
move /y %ANDROID_BUILD_PATH%\..\libstrophe.a %ANDROID_LIB_PATH%\Release\armv7
popd
rmdir "%ANDROID_BUILD_PATH%" /s/q

@echo Building for Android Arm7 (32-bit) Debug
mkdir "%ANDROID_BUILD_PATH%"
pushd "%ANDROID_BUILD_PATH%"
REM Generate Project Files
set UE4_OPENSSL_ARCH=armv7
cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE="%PATH_TO_CMAKE_FILE%\Android\Android.cmake" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM=%MAKE% -DCMAKE_BUILD_TYPE=Debug -DANDROID_NATIVE_API_LEVEL=android-19 -DANDROID_ABI=armeabi-v7a -DANDROID_STL=gnustl_shared -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%\\lib" %PATH_TO_CMAKE_FILE%
REM Now compile it
%MAKE%
if not exist "%ANDROID_LIB_PATH%\Debug\armv7" mkdir "%ANDROID_LIB_PATH%\Debug\armv7"
move /y %ANDROID_BUILD_PATH%\..\libstrophe.a %ANDROID_LIB_PATH%\Debug\armv7
popd
rmdir "%ANDROID_BUILD_PATH%" /s/q

REM done
goto Exit

:ExpatMissing
echo Could not find UE4 Expat. Please check your UE4_EXPAT_ROOT_DIR environment variable and try again.
goto Exit

:NDKRootMissing
echo Could not find Android NDK Root. Please check your NDKROOT environment variable and try again.
goto Exit

:Exit
endlocal
