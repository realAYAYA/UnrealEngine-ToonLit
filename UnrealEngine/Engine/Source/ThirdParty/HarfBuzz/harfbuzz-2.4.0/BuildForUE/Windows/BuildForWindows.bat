@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set VS2015_X86_PATH="%PATH_TO_CMAKE_FILE%\..\lib\Win32\VS2015\Build"
set VS2015_X64_PATH="%PATH_TO_CMAKE_FILE%\..\lib\Win64\VS2015\Build"

REM Build for VS2015 (32-bit)
echo Generating HarfBuzz solution for VS2015 (32-bit)...
if exist %VS2015_X86_PATH% (rmdir %VS2015_X86_PATH% /s/q)
mkdir %VS2015_X86_PATH%
cd %VS2015_X86_PATH%
cmake -G "Visual Studio 14 2015" %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz solution for VS2015 (32-bit, Debug)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Debug
echo Building HarfBuzz solution for VS2015 (32-bit, Release)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Release
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2015_X86_PATH%\harfbuzz.dir\Debug\harfbuzz.pdb" "%VS2015_X86_PATH%\..\Debug\harfbuzz.pdb"
rmdir %VS2015_X86_PATH% /s/q

REM Build for VS2015 (64-bit)
echo Generating HarfBuzz solution for VS2015 (64-bit)...
if exist %VS2015_X64_PATH% (rmdir %VS2015_X64_PATH% /s/q)
mkdir %VS2015_X64_PATH%
cd %VS2015_X64_PATH%
cmake -G "Visual Studio 14 2015 Win64" %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz solution for VS2015 (64-bit, Debug)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Debug
echo Building HarfBuzz solution for VS2015 (64-bit, Release)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Release
echo Building HarfBuzz solution for VS2015 (64-bit, RelWithDebInfo)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2015_X64_PATH%\harfbuzz.dir\Debug\harfbuzz.pdb" "%VS2015_X64_PATH%\..\Debug\harfbuzz.pdb"
copy /B/Y "%VS2015_X64_PATH%\harfbuzz.dir\RelWithDebInfo\harfbuzz.pdb" "%VS2015_X64_PATH%\..\RelWithDebInfo\harfbuzz.pdb"
rmdir %VS2015_X64_PATH% /s/q

endlocal
