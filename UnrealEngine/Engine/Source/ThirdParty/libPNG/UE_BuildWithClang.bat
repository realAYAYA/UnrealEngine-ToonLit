REM Copyright Epic Games, Inc. All Rights Reserved.
@echo off

SETLOCAL

REM This build script relies on VS2019 being present with clang support installed
CALL "%VS2019INSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"

REM Release Build

REM prepare output folder and move to it
if not exist "libPNG-1.5.2\lib\Win64-llvm\Release" mkdir libPNG-1.5.2\lib\Win64-llvm\Release
pushd libPNG-1.5.2\lib\Win64-llvm\Release

REM make sure everything is properly regenerated
if exist "CMakeCache.txt" del /Q CMakeCache.txt

REM clang and cmake must also be installed and available in your environment path
cmake -G "Ninja" -DPNG_SHARED:BOOL="False" -DCMAKE_C_COMPILER:FILEPATH="clang-cl.exe" -DCMAKE_C_FLAGS_RELWITHDEBINFO:STRING="/MD /Zi /O2 /Ob2 /DNDEBUG /Qvec" -DZLIB_INCLUDE_DIR=..\..\..\..\..\zlib\v1.2.8d\include\Win64\VS2015 -DZLIB_LIBRARY_RELEASE=..\..\..\..\..\zlib\v1.2.8d\lib\Win64\VS2015\Release\zlibstatic.lib -DCMAKE_BUILD_TYPE="RelWithDebInfo" ..\..\..

REM clean and build
ninja clean
ninja

popd

REM Debug Build

REM prepare output folder and move to it
if not exist "libPNG-1.5.2\lib\Win64-llvm\Debug" mkdir libPNG-1.5.2\lib\Win64-llvm\Debug
pushd libPNG-1.5.2\lib\Win64-llvm\Debug

REM make sure everything is properly regenerated
if exist "CMakeCache.txt" del /Q CMakeCache.txt

REM clang and cmake must also be installed and available in your environment path
cmake -G "Ninja" -DPNG_SHARED:BOOL="False" -DCMAKE_C_COMPILER:FILEPATH="clang-cl.exe" -DCMAKE_C_FLAGS_DEBUG:STRING="/MDd /Zi /Ob0 /Od /RTC1 /Qvec" -DZLIB_INCLUDE_DIR=..\..\..\..\..\zlib\v1.2.8d\include\Win64\VS2015 -DZLIB_LIBRARY_DEBUG=..\..\..\..\..\zlib\v1.2.8d\lib\Win64\VS2015\Debug\zlibstatic.lib -DCMAKE_BUILD_TYPE="Debug" ..\..\..

REM clean and build
ninja clean
ninja

popd


ENDLOCAL
