
@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

if not exist "%LINUX_MULTIARCH_ROOT%" (
	echo LINUX_MULTIARCH_ROOT not defined. Either manually export the path to your Linux toolchain root or set an environment variable to the path.
	echo This can happen if you are running this script as a stand alone.
	exit /B -1
)

set ENGINEROOT=%CD%\..\..\..\..\..\..\..\..\..
set CMAKESOURCE=%CD%\..\..
REM The Unix folder is what is used for Linux based systems that UE supports 
set LIBOUTPUTDIRECTORY=%cd%\..\..\..\lib\Unix
set BUILDDIRECTORY=%LIBOUTPUTDIRECTORY%\Build
set TOOLCHAINDIRECTORY=%ENGINEROOT%\Source\ThirdParty\CMake\PlatformScripts\Linux 
set MAKEEXECUTABLE=%ENGINEROOT%\Extras\ThirdPartyNotUE\GNU_Make\make-3.81\bin\make.exe
set OutputFileName=libFlite.a

REM Remove Build folder and recreate it to clear it out
if exist "%BUILDDIRECTORY%" (rmdir "%BUILDDIRECTORY%" /s/q)
mkdir "%BUILDDIRECTORY%"



REM It's really important that the architectures pass in match the toolchain files in Engine/Source/ThirdParty/CMake/PlatformScripts/Linux
REM The toolchain files follow Linux-<ARCHITECTURE>.cmake. Copy that architecture here as an argument 
call :BuildLib aarch64-unknown-linux-gnueabi Debug
call :BuildLib aarch64-unknown-linux-gnueabi Release
call :BuildLib x86_64-unknown-linux-gnu Debug
call :BuildLib x86_64-unknown-linux-gnu Release

REM We cd to the lib directory to be able to remove the build directory successfully 
cd %LIBOUTPUTDIRECTORY%
REM Cleanup the Build Directory 
if exist "%BUILDDIRECTORY%" (rmdir "%BUILDDIRECTORY%" /s/q)
endlocal
exit /B 0

REM Argument order: 1. Architecture, 2. Debug/Release
:BuildLib
	pushd

	echo Building Flite makefile for %~1(%~2)
	set LINUXARCHITECTURE=%~1
	set BuildType=%~2
	set TOOLCHAINFILE=%TOOLCHAINDIRECTORY%\Linux-%LINUXARCHITECTURE%.cmake
	REM We explicitly specify the compilers so cmake doesn't auto detect the compiler to be used and use the wrong compiler for an architecture
	set LINUXCCOMPILER=%LINUX_MULTIARCH_ROOT%%LINUXARCHITECTURE%\bin\clang.exe
	set LINUXCXXCOMPILER=%LINUX_MULTIARCH_ROOT%%LINUXARCHITECTURE%\bin\clang++.exe
	set DestFolder=%LIBOUTPUTDIRECTORY%\%LINUXARCHITECTURE%\%BuildType%
	if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
	mkdir "%DestFolder%"

	set CurrentLINUXBuildDir=%BUILDDIRECTORY%\%LINUXARCHITECTURE%\%BuildType%
	mkdir "%CurrentLINUXBuildDir%"
	cd %CurrentLINUXBuildDir%
	cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAINFILE%" -DCMAKE_MAKE_PROGRAM="%MAKEEXECUTABLE%" -DCMAKE_C_COMPILER="%LINUXCCOMPILER%" -DCMAKE_CXX_COMPILER="%LINUXCXXCOMPILER%" -DCMAKE_BUILD_TYPE=%BuildType% %CMAKESOURCE% 
	"%MAKEEXECUTABLE%" -j 16 MAKE="%MAKEEXECUTABLE% -j 16"
	REM The linux toolchain appends another "lib" to the start of the lib. We remove that here 
	echo "%cd%/..\lib%OutputFileName%"
	move /y "%cd%/..\lib%OutputFileName%" "%DestFolder%\%OutputFileName%"
	
	popd
	exit /B 0
