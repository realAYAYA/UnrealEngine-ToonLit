
@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

if not exist "%NDKROOT%" (
	echo NDKROOT is undefined. Either manually export the path or set an environment variable that points to the path.
	echo This can happen if you are running this script in stand alone instead of through UBT.
	exit /B -1
)

set ENGINEROOT=%CD%\..\..\..\..\..\..\..\..\..
set CMAKESOURCE=%CD%\..\..
set LIBOUTPUTDIRECTORY=%cd%\..\..\..\lib\Android
set BUILDDIRECTORY=%LIBOUTPUTDIRECTORY%\Build
set TOOLCHAINDIRECTORY=%ENGINEROOT%\Source\ThirdParty\CMake\PlatformScripts\Android 
set MAKEEXECUTABLE=%NDKROOT%\prebuilt\windows-x86_64\bin\make.exe

REM Remove Build folder and recreate it to clear it out
if exist "%BUILDDIRECTORY%" (rmdir "%BUILDDIRECTORY%" /s/q)
mkdir "%BUILDDIRECTORY%"

REM Set any additional flags here
set CXXFLAGS="-std=c++14 -Wno-sign-compare -DU_DISABLE_VERSION_SUFFIX=0"
set AdditionalCompileArguments=

set OutputFileName=libFlite.a

set AndroidAPILevel=android-21
call :BuildLib ARM64 Debug
call :BuildLib ARM64 Release
call :BuildLib x64 Debug
call :BuildLib x64 Release

REM We cd to the lib directory to be able to remove the build directory successfully 
cd %LIBOUTPUTDIRECTORY%
REM Cleanup the Build Directory 
if exist "%BUILDDIRECTORY%" (rmdir "%BUILDDIRECTORY%" /s/q)

endlocal
exit /B 0

REM Argument order: 1. Architecture (ARM64/x64), 2. Debug/Release
:BuildLib
	pushd

	echo Building Flite makefile for %~1(%~2)
	set ANDROIDARCHITECTURE=%~1
	set AndroidABI=arm64-v8a
	if "%ANDROIDARCHITECTURE%" == "x64" (
		set AndroidABI=x86_64
	)
	

	set BuildType=%~2
	set TOOLCHAINFILE=%TOOLCHAINDIRECTORY%\Android-%ANDROIDARCHITECTURE%.cmake
	set DestFolder=%LIBOUTPUTDIRECTORY%\%ANDROIDARCHITECTURE%\%BuildType%
	if exist "%DestFolder%" (rmdir "%DestFolder%" /s/q)
	mkdir "%DestFolder%"

	set CurrentAndroidBuildDir=%BUILDDIRECTORY%\%AndroidABI%\%BuildType%
	mkdir "%CurrentAndroidBuildDir%"
	cd %CurrentAndroidBuildDir%
	cmake -G"MinGW Makefiles" -fPIC -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAINFILE%" -DANDROID_NDK="%NDKROOT%" -DCMAKE_MAKE_PROGRAM="%MAKEEXECUTABLE%" -DANDROID_NATIVE_API_LEVEL="%AndroidAPILevel%" -DANDROID_ABI="%AndroidABI%" -DANDROID_STL="c++_shared" -DCMAKE_BUILD_TYPE=%BuildType% -DCMAKE_CXX_FLAGS=%CXXFLAGS% %AdditionalCompileArguments% %CMAKESOURCE% 
	cmake --build .
	move /y "%cd%/..\%OutputFileName%" "%DestFolder%\%OutputFileName%"
	
	popd
	exit /B 0
