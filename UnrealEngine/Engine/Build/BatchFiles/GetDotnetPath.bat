@echo off

rem ## Unreal Engine utility script
rem ## Copyright Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script verifies that dotnet sdk is installed and a new enough SDK is present, alternatively setups up the submitted SDK for use.

rem if UE_USE_SYSTEM_DOTNET we assume a installed dotnet is present
if "%UE_USE_SYSTEM_DOTNET%" == "1" goto verify_dotnet

rem if UE_DOTNET_VERSION is already set we assume this script has already run
if "%UE_DOTNET_VERSION%" == "6.0.302" goto Succeeded

rem figure out which architecture to use
set UE_DOTNET_ARCH=windows
if "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
	set UE_DOTNET_ARCH=win-arm64
)

rem add the dotnet sdk in the repo as the current dotnet sdk
set UE_DOTNET_VERSION=6.0.302
set UE_DOTNET_DIR=%~dp0..\..\Binaries\ThirdParty\DotNet\%UE_DOTNET_VERSION%\%UE_DOTNET_ARCH%
set PATH=%UE_DOTNET_DIR%;%PATH%
set DOTNET_ROOT=%UE_DOTNET_DIR%

rem Disables use of any installed version of dotnet
set DOTNET_MULTILEVEL_LOOKUP=0

rem for dotnet applications that require older dotnet runtimes, this will allow them to use our bundled dotnet runtime
set DOTNET_ROLL_FORWARD=LatestMajor

rem skip dotnet verification when using our submitted sdk as we know it is up to date
ECHO Using bundled DotNet SDK version: %UE_DOTNET_VERSION%
goto Succeeded

:verify_dotnet

for /f "delims=" %%i in ('where dotnet') do (
	REM Dotnet exists
	goto find_sdks
)

REM Dotnet command did not exist
exit /B 1

:find_sdks

set REQUIRED_MAJOR_VERSION=6
set REQUIRED_MINOR_VERSION=0

set FOUND_MAJOR=
set FOUND_MINOR=
REM Unfortunately dotnet lists the sdks in oldest version first, thus we will pick the oldest version that matches our criteria as valid.
REM This does not really matter as we are just trying to verify that a new enough SDK is actually present 
for /f "tokens=1,* delims= " %%I in ('dotnet --list-sdks') do (

	for /f "tokens=1,2,3 delims=." %%X in ("%%I") do (
		REM We can check the patch version for preview versions and ignore those, but it slowed down this batch to much so accepting those for now. 
		REM We do not actually use the determined version for anything so usually the newest SDK installed is used anyway
		if %%X EQU %REQUIRED_MAJOR_VERSION% (
			REM If the major version is the same as we require we check the minor version
			if %%Y GEQ %REQUIRED_MINOR_VERSION% (

				set FOUND_MAJOR=%%X
				set FOUND_MINOR=%%Y
				ECHO Found Dotnet SDK version: %%X.%%Y.%%Z
				goto Succeeded
			)
		)

		if %%X GTR %REQUIRED_MAJOR_VERSION% (
			REM If the major version is greater then what we require then this sdk is good enough
			set FOUND_MAJOR=%%X
			set FOUND_MINOR=%%Y
			ECHO Found Dotnet SDK version: %%X.%%Y.%%Z
			goto Succeeded
		)

	)

)

REM Dotnet is installed but the sdk present is to old
exit /B 1

:Succeeded
exit /B 0
	
