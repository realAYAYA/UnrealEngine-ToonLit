@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\..\

REM Temporary build directories (used as working directories when running CMake)
set VS2019_X64_PATH="%PATH_TO_CMAKE_FILE%\..\lib\Win64\VS2019\Build"

REM Build for VS2019 (64-bit)
echo Generating Flite solution for VS2019 (64-bit)...
if exist %VS2019_X64_PATH% (rmdir %VS2019_X64_PATH% /s/q)
mkdir %VS2019_X64_PATH%
cd %VS2019_X64_PATH%
cmake -G "Visual Studio 16 2019" -A x64 %PATH_TO_CMAKE_FILE%
echo Building Flite solution for VS2019 (64-bit, Debug)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build Debug
echo Building Flite solution for VS2019 (64-bit, Release)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build Release
echo Building Flite solution for VS2019 (64-bit, RelWithDebInfo)...
"%VS160COMNTOOLS%\..\IDE\devenv.exe" flite.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2019_X64_PATH%\libFlite.dir\Debug\libFlite.pdb" "%VS2019_X64_PATH%\..\Debug\libFlite.pdb"
copy /B/Y "%VS2019_X64_PATH%\libFlite.dir\RelWithDebInfo\libFlite.pdb" "%VS2019_X64_PATH%\..\RelWithDebInfo\libFlite.pdb"
rmdir %VS2019_X64_PATH% /s/q

endlocal
