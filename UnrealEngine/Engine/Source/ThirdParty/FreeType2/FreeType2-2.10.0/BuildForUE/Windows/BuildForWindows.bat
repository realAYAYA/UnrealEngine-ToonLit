@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\..\

set PATH_TO_ZLIB=%CD%\..\..\..\..\zlib\v1.2.8
set PATH_TO_ZLIB_WIN32_SRC=%PATH_TO_ZLIB%\include\Win32\VS2015
set PATH_TO_ZLIB_WIN64_SRC=%PATH_TO_ZLIB%\include\Win64\VS2015
set PATH_TO_ZLIB_DUMMY_LIB=%PATH_TO_ZLIB%\lib\Win32\VS2015\Debug

set PATH_TO_PNG=%CD%\..\..\..\..\libPNG\libPNG-1.5.2
set PATH_TO_PNG_SRC=%PATH_TO_PNG%
set PATH_TO_PNG_DUMMY_LIB=%PATH_TO_PNG%\lib\Win32\VS2015

REM Temporary build directories (used as working directories when running CMake)
set VS2015_X86_PATH="%PATH_TO_CMAKE_FILE%\lib\Win32\VS2015\Build"
set VS2015_X64_PATH="%PATH_TO_CMAKE_FILE%\lib\Win64\VS2015\Build"

REM Build for VS2015 (32-bit)
echo Generating FreeType solution for VS2015 (32-bit)...
if exist %VS2015_X86_PATH% (rmdir %VS2015_X86_PATH% /s/q)
mkdir %VS2015_X86_PATH%
cd %VS2015_X86_PATH%
cmake -G "Visual Studio 14 2015" -DFT_WITH_ZLIB=ON -DFT_WITH_PNG=ON -DCMAKE_PREFIX_PATH="%PATH_TO_ZLIB_WIN32_SRC%;%PATH_TO_ZLIB_DUMMY_LIB%;%PATH_TO_PNG_SRC%;%PATH_TO_PNG_DUMMY_LIB%" %PATH_TO_CMAKE_FILE%
echo Building FreeType solution for VS2015 (32-bit, Debug)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" freetype.sln /Build Debug
echo Building FreeType solution for VS2015 (32-bit, Release)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" freetype.sln /Build Release
cd %PATH_TO_CMAKE_FILE%
xcopy "%VS2015_X86_PATH%\Debug" "%VS2015_X86_PATH%\..\Debug" /i/y/q
xcopy "%VS2015_X86_PATH%\Release" "%VS2015_X86_PATH%\..\Release" /i/y/q
copy /B/Y "%VS2015_X86_PATH%\freetype.dir\Debug\freetype.pdb" "%VS2015_X86_PATH%\..\Debug\freetype.pdb"
rmdir %VS2015_X86_PATH% /s/q

REM Build for VS2015 (64-bit)
echo Generating FreeType solution for VS2015 (64-bit)...
if exist %VS2015_X64_PATH% (rmdir %VS2015_X64_PATH% /s/q)
mkdir %VS2015_X64_PATH%
cd %VS2015_X64_PATH%
cmake -G "Visual Studio 14 2015 Win64" -DFT_WITH_ZLIB=ON -DFT_WITH_PNG=ON -DCMAKE_PREFIX_PATH="%PATH_TO_ZLIB_WIN64_SRC%;%PATH_TO_ZLIB_DUMMY_LIB%;%PATH_TO_PNG_SRC%;%PATH_TO_PNG_DUMMY_LIB%" %PATH_TO_CMAKE_FILE%
echo Building FreeType solution for VS2015 (64-bit, Debug)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" freetype.sln /Build Debug
echo Building FreeType solution for VS2015 (64-bit, Release)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" freetype.sln /Build Release
echo Building FreeType solution for VS2015 (64-bit, RelWithDebInfo)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" freetype.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
xcopy "%VS2015_X64_PATH%\Debug" "%VS2015_X64_PATH%\..\Debug" /i/y/q
xcopy "%VS2015_X64_PATH%\Release" "%VS2015_X64_PATH%\..\Release" /i/y/q
xcopy "%VS2015_X64_PATH%\RelWithDebInfo" "%VS2015_X64_PATH%\..\RelWithDebInfo" /i/y/q
copy /B/Y "%VS2015_X64_PATH%\freetype.dir\Debug\freetype.pdb" "%VS2015_X64_PATH%\..\Debug\freetype.pdb"
copy /B/Y "%VS2015_X64_PATH%\freetype.dir\RelWithDebInfo\freetype.pdb" "%VS2015_X64_PATH%\..\RelWithDebInfo\freetype.pdb"
rmdir %VS2015_X64_PATH% /s/q

endlocal
