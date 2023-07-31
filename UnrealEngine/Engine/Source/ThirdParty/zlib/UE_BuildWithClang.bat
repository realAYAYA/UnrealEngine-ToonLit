REM Copyright Epic Games, Inc. All Rights Reserved.
@echo off

SETLOCAL

SET ZLIBVERSION=zlib-1.2.8
SET ZLIBSHA256=36658cb768a54c1d4dec43c3116c27ed893e88b02ecfcb44f2166f9c0b7f2a0d
SET ZLIBFOLDER=v1.2.8

REM This build script relies on VS2019 being present with clang support installed
CALL "%VS2019INSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"

IF NOT EXIST "%ZLIBFOLDER%\build\src" mkdir %ZLIBFOLDER%\build\src

REM validate checksum using Windows 10 builtin tools
certUtil -hashfile %ZLIBFOLDER%\build\%ZLIBVERSION%.tar.gz SHA256 | findstr %ZLIBSHA256%

REM errorlevel set by findstr should be 0 when the text has been found
IF %ERRORLEVEL% NEQ 0 (
   echo Checksum failed on %ZLIBVERSION%.tar.gz
   goto :exit
)

echo %ZLIBVERSION%.tar.gz SHA256 checksum is valid

REM tar is now present by default on Windows 10
tar -xf %ZLIBFOLDER%\build\%ZLIBVERSION%.tar.gz -C %ZLIBFOLDER%\build\src

REM apply our patched version of CMakeLists that contains the minizip contrib files in the static library for Windows
IF EXIST "%ZLIBFOLDER%\build\CMakeLists.txt" copy /Y "%ZLIBFOLDER%\build\CMakeLists.txt" %ZLIBFOLDER%\build\src\%ZLIBVERSION%

REM prepare output folder and move to it
if not exist "%ZLIBFOLDER%\lib\Win64-llvm\Release" mkdir %ZLIBFOLDER%\lib\Win64-llvm\Release
pushd %ZLIBFOLDER%\lib\Win64-llvm\Release

REM make sure everything is properly regenerated
if exist "CMakeCache.txt" del /Q CMakeCache.txt

REM clang and cmake must also be installed and available in your environment path
cmake -G "Ninja" -DCMAKE_C_COMPILER:FILEPATH="clang-cl.exe" -DCMAKE_C_FLAGS_RELWITHDEBINFO:STRING="/MD /Zi /O2 /Ob2 /DNDEBUG /Qvec" -DCMAKE_BUILD_TYPE="RelWithDebInfo" ..\..\..\build\src\%ZLIBVERSION%

REM clean and build
ninja clean
ninja

popd

if not exist "%ZLIBFOLDER%\lib\Win64-llvm\Debug" mkdir %ZLIBFOLDER%\lib\Win64-llvm\Debug
pushd %ZLIBFOLDER%\lib\Win64-llvm\Debug

REM make sure everything is properly regenerated
if exist "CMakeCache.txt" del /Q CMakeCache.txt

REM clang and cmake must also be installed and available in your environment path
cmake -G "Ninja" -DCMAKE_C_COMPILER:FILEPATH="clang-cl.exe" -DCMAKE_C_FLAGS_DEBUG:STRING="/MDd /Zi /Ob0 /Od /RTC1" -DCMAKE_BUILD_TYPE="Debug" ..\..\..\build\src\%ZLIBVERSION%

REM clean and build
ninja clean
ninja

popd

:exit

ENDLOCAL
