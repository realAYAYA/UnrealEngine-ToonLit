@echo off

rem ## Unreal Engine code build script
rem ## Copyright Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script is expecting to exist in the /Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.
rem ##
rem ##     %1 is the game name
rem ##     %2 is the platform name
rem ##     %3 is the configuration name
rem ##     additional args are passed directly to UnrealBuildTool

setlocal enabledelayedexpansion

rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
rem ## verify that our relative path to the /Engine/Source directory is correct
if not exist "%~dp0..\..\Source" goto Error_BatchFileInWrongLocation

rem ## Change the CWD to /Engine/Source.  We always need to run UnrealBuildTool from /Engine/Source!
pushd "%~dp0\..\..\Source"
if not exist ..\Build\BatchFiles\Build.bat goto Error_BatchFileInWrongLocation

set UBTPath="..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"

rem ## Verify that dotnet is present
call "%~dp0GetDotnetPath.bat"
if errorlevel 1 goto Error_NoDotnetSDK
REM ## Skip msbuild detection if using dotnet as this is done for us by dotnet-cli

rem ## Compile UBT if the project file exists
:ReadyToBuildUBT
set ProjectFile="Programs\UnrealBuildTool\UnrealBuildTool.csproj"
if not exist %ProjectFile% goto NoProjectFile

rem ## Only build if UnrealBuildTool.dll is missing, as Visual Studio or GenerateProjectFiles should be building UnrealBuildTool
rem ## Note: It is possible UnrealBuildTool will be out of date if the solution was generated with -NoDotNet or is VS2019
rem ##       Historically this batch file did not compile UnrealBuildTool
if not exist %UBTPath% (
	rem ## If this script was called from Visual Studio 2022, build UBT with Visual Studio to prevent unnecessary rebuilds.
	if "%VisualStudioVersion%" GEQ "17.0" (
		echo Building UnrealBuildTool with %VisualStudioEdition%...
		"%VSAPPIDDIR%..\..\MSBuild\Current\Bin\MSBuild.exe" %ProjectFile% -t:Build -p:Configuration=Development -verbosity:quiet -noLogo
		if errorlevel 1 goto Error_UBTCompileFailed
	) else (
		echo Building UnrealBuildTool with dotnet...
		dotnet build %ProjectFile% -c Development -v quiet
		if errorlevel 1 goto Error_UBTCompileFailed
	)
)
:NoProjectFile

rem ## Run UBT
:ReadyToBuild
if not exist %UBTPath% goto Error_UBTMissing
echo Running UnrealBuildTool: dotnet %UBTPath% %*
dotnet %UBTPath% %*
EXIT /B !ERRORLEVEL!

:Error_BatchFileInWrongLocation
echo ERROR: The batch file does not appear to be located in the Engine/Build/BatchFiles directory. This script must be run from within that directory.
EXIT /B 999

:Error_NoDotnetSDK
echo ERROR: Unable to find an install of Dotnet SDK.  Please make sure you have it installed and that `dotnet` is a globally available command.
EXIT /B 999

:Error_UBTCompileFailed
echo ERROR: Failed to build UnrealBuildTool.
EXIT /B 999

:Error_UBTMissing
echo ERROR: UnrealBuildTool.dll not found in %UBTPath%
EXIT /B 999
