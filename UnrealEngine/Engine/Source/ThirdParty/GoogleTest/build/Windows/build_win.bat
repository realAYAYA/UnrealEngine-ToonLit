@echo off
REM * build Google Test as static lib
REM * unzip google-test-source.7z in place to '\ThirdParty\GoogleTest\build\google-test-source' before running

set GTEST_SDK=%cd%\..\google-test-source

set PROGFILES=%ProgramFiles%
if not "%ProgramFiles(x86)%" == "" set PROGFILES=%ProgramFiles(x86)%

set SHAREDLIBS=OFF
if "%4" == "Shared" set SHAREDLIBS=ON

REM select build configuration (default to MinSizeRel) other possible configurations include: Debug, Release, RelWithDebInfo
set CONFIG=MinSizeRel
if not "%3" == "" set CONFIG=%3%

REM select architecture (default to 64 bit)
set ARCH_VER=x64
if not "%2" == "" set ARCH_VER=%2%

REM select compiler (default to 2015)
set COMPILER_VER=2015
if not "%1" == "" set COMPILER_VER=%1

set CMAKE_GENERATOR_ARGS=
set MSBUILD_PLATFORM_ARG=PlatformTarget

if "%COMPILER_VER%" == "2013" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 12.0"
	set VCVERSION=12
	set MSBUILDDIR="%PROGFILES%\MSBuild\12.0\Bin"
	
	if "%ARCH_VER%" == "x64" (
		set CMAKE_GENERATOR="Visual Studio 12 2013 Win64"
	)
	
	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR="Visual Studio 12 2013"
	)
)
if "%COMPILER_VER%" == "2015" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 14.0"
	set VCVERSION=14
	set MSBUILDDIR="%PROGFILES%\MSBuild\14.0\Bin"

	if "%ARCH_VER%" == "x64" (
		set CMAKE_GENERATOR="Visual Studio 14 2015 Win64"
	)
	
	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR="Visual Studio 14 2015"
	)
)

if "%COMPILER_VER%" == "2017" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio\2017\Professional"
	set VCVERSION=15
	set MSBUILDDIR="%PROGFILES%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin"

	if "%ARCH_VER%" == "x64" (
		set CMAKE_GENERATOR="Visual Studio 15 2017 Win64"
	)
	
	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR="Visual Studio 15 2017"
	)
)

if "%COMPILER_VER%" == "2019" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio\2019\Professional"
	set VCVERSION=16
	set MSBUILDDIR="%PROGFILES%\Microsoft Visual Studio\2019\Professional\MSBuild\15.0\Bin"
	set MSBUILD_PLATFORM_ARG=Platform
	set CMAKE_GENERATOR_ARGS="-A %ARCH_VER%"

	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR_ARGS="-A Win32"
	)
)

if "%COMPILER_VER%" == "2022" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio\2022\Professional"
	set VCVERSION=17
	set MSBUILDDIR="%PROGFILES%\Microsoft Visual Studio\2022\Professional\MSBuild\15.0\Bin"
	set MSBUILD_PLATFORM_ARG=Platform
	set CMAKE_GENERATOR="Visual Studio 17 2022"
	set CMAKE_GENERATOR_ARGS="-A %ARCH_VER%"
	
	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR_ARGS="-A Win32"
	)
	
	if "%ARCH_VER:~0,3%"=="ARM" (
		set CMAKE_GENERATOR_ARGS="-A ARM64EC"
	)
)

REM setup output directory
set OUTPUT_DIR=%cd%\Artifacts_VS%COMPILER_VER%_%ARCH_VER%_%CONFIG%
if "%4" == "Shared" (
	set OUTPUT_DIR=%OUTPUT_DIR%_Shared
)

rmdir /s /q %OUTPUT_DIR%
mkdir %OUTPUT_DIR%

REM ensure source has been unpacked
if not exist %GTEST_SDK% (
	pushd %cd%\..\
	call uncompress_and_patch
	popd
)

REM config cmake project
pushd %OUTPUT_DIR%
cmake -D BUILD_SHARED_LIBS:BOOL=%SHAREDLIBS% -D gtest_force_shared_crt:BOOL=ON -D gtest_disable_pthreads:BOOL=ON -G %CMAKE_GENERATOR% %CMAKE_GENERATOR_ARGS% %GTEST_SDK%
popd

if "%ARCH_VER:~0,3%"=="ARM" (
	echo ----------------------------------------------------------------------
	echo NOTE: You can't currently build an ARM64X lib without modifying the solution after it is generated.
	echo After the ARM64EC solution is generated, open it, create a ARM64 platfrom in the solution by copying the ARM64EC settings,
	echo update the "Build Project as ARM64X" property to true while ARM64EC is the active platform and compile.
	echo More information can be found here: https://learn.microsoft.com/en-us/windows/arm/arm64x-build
	echo ----------------------------------------------------------------------
	EXIT /B 0
)

REM build project
pushd %MSBUILDDIR%
msbuild.exe %OUTPUT_DIR%\googletest-distribution.sln /target:ALL_BUILD /p:%MSBUILD_PLATFORM_ARG%=%ARCH_VER%;Configuration="%CONFIG%"
popd

REM setup binary output directory
set OUTPUT_LIBS=%cd%\..\..\lib\Win64\VS%COMPILER_VER%\%CONFIG%
if "%ARCH_VER%" == "x86" (
	set OUTPUT_LIBS=%cd%\..\..\lib\Win32\VS%COMPILER_VER%\%CONFIG%
)

if "%4" == "Shared" (
	set OUTPUT_LIBS=%OUTPUT_LIBS%_Shared
)

REM delete any existing library output directories
if exist %OUTPUT_LIBS% rmdir /s /q %OUTPUT_LIBS%
mkdir %OUTPUT_LIBS%

REM copy binaries
xcopy /s /c /d /y %OUTPUT_DIR%\lib\%CONFIG% %OUTPUT_LIBS%




