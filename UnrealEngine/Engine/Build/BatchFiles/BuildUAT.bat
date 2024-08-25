@echo off

rem ## Unreal Engine AutomationTool build script
rem ## Copyright Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script is expecting to exist in the Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

setlocal

rem ## Make sure we use the Windows "find" utility and not a Unix-like tool found in PATH
set FIND=%SYSTEMROOT%\System32\find.exe

rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
rem ## verify that our relative path to the /Engine/Source directory is correct
if not exist "%~dp0..\..\Source" goto Error_BatchFileInWrongLocation

rem ## Change the CWD to /Engine/Source.
pushd "%~dp0..\..\Source"
if not exist ..\Build\BatchFiles\BuildUAT.bat goto Error_BatchFileInWrongLocation

rem Check to see if the files in the AutomationTool, EpicGames.Build, EpicGames.Core, or UnrealBuildTool directory have changed.
rem find ".cs" files to only lines that match those names - excludes lines that will change for uninteresting reasons, like free space
md ..\Intermediate\Build >nul 2>nul

dir /s ^
 Programs\Shared\EpicGames.Build\*.cs ^
 Programs\Shared\EpicGames.Build\*.csproj ^
 Programs\Shared\EpicGames.Core\*.cs ^
 Programs\Shared\EpicGames.Core\*.csproj ^
 Programs\Shared\EpicGames.Horde\*.cs ^
 Programs\Shared\EpicGames.Horde\*.csproj ^
 Programs\Shared\EpicGames.IoHash\*.cs ^
 Programs\Shared\EpicGames.IoHash\*.csproj ^
 Programs\Shared\EpicGames.MsBuild\*.cs ^
 Programs\Shared\EpicGames.MsBuild\*.csproj ^
 Programs\Shared\EpicGames.OIDC\*.cs ^
 Programs\Shared\EpicGames.OIDC\*.csproj ^
 Programs\Shared\EpicGames.Serialization\*.cs ^
 Programs\Shared\EpicGames.Serialization\*.csproj ^
 Programs\Shared\EpicGames.UBA\*.cs ^
 Programs\Shared\EpicGames.UBA\*.csproj ^
 Programs\Shared\EpicGames.UHT\*.cs ^
 Programs\Shared\EpicGames.UHT\*.csproj ^
 Programs\UnrealBuildTool\*.cs ^
 Programs\UnrealBuildTool\*.csproj ^
 | %FIND% ".cs" > ..\Intermediate\Build\AutomationToolFiles.txt

if exist ..\Binaries\Win64\UnrealBuildAccelerator (
	dir /s ^
	 ..\Binaries\Win64\UnrealBuildAccelerator\*.dll ^
	 | %FIND% ".dll" >> ..\Intermediate\Build\AutomationToolFiles.txt
	dir /s ^
	 ..\Binaries\Win64\UnrealBuildAccelerator\*.exe ^
	 | %FIND% ".exe" >> ..\Intermediate\Build\AutomationToolFiles.txt
) 2>nul

if not exist ..\Platforms goto NoPlatforms
for /d %%D in (..\Platforms\*) do (
	for %%F in (AutomationTool Shared UnrealBuildTool) do (
		if exist %%D\Source\Programs\%%F (
			dir /s ^
			%%D\Source\Programs\%%F\*.cs ^
			%%D\Source\Programs\%%F\*.csproj ^
			| %FIND% ".cs" >> ..\Intermediate\Build\AutomationToolFiles.txt
		) 2>nul
	)
)
:NoPlatforms

if not exist ..\Restricted goto NoRestricted
for /d %%D in (..\Restricted\*) do (
	for %%F in (AutomationTool Shared UnrealBuildTool) do (
		if exist %%D\Source\Programs\%%F (
			dir /s ^
			%%D\Source\Programs\%%F\*.cs ^
			%%D\Source\Programs\%%F\*.csproj ^
			| %FIND% ".cs" >> ..\Intermediate\Build\AutomationToolFiles.txt
		) 2>nul
	)
)
:NoRestricted

rem note: no /s
dir ^
 Programs\Shared\MetaData.cs ^
 Programs\AutomationTool\*.cs ^
 Programs\AutomationTool\*.csproj ^
 | %FIND% ".cs" >>..\Intermediate\Build\AutomationToolFiles.txt

set MSBUILD_LOGLEVEL=%1
if not defined %MSBUILD_LOGLEVEL set MSBUILD_LOGLEVEL=quiet

set ARGUMENT=%2
if not defined %ARGUMENT goto Check_UpToDate
if /I "%ARGUMENT%" == "FORCE" goto Build_AutomationTool

:Check_UpToDate
if not exist ..\Binaries\DotNET\AutomationTool\AutomationTool.dll goto Build_AutomationTool
set RUNUAT_EXITCODE=0
rem per https://ss64.com/nt/fc.html using redirection syntax rather than errorlevel, based on observed inconsistent results from this function
fc ..\Intermediate\Build\AutomationToolFiles.txt ..\Intermediate\Build\AutomationToolPrevFiles.txt >nul 2>&1 && goto Exit

:Build_AutomationTool
rem ## Verify that dotnet is present
call "%~dp0GetDotnetPath.bat"
if errorlevel 1 goto Error_NoDotnetSDK

echo Building AutomationTool...
dotnet build Programs\AutomationTool\AutomationTool.csproj -c Development -v %MSBUILD_LOGLEVEL%
if errorlevel 1 goto Error_UATCompileFailed

rem record input files - regardless of how we got here, these are now our point of reference
copy /y ..\Intermediate\Build\AutomationToolFiles.txt ..\Intermediate\Build\AutomationToolPrevFiles.txt >nul

goto Exit


:Error_BatchFileInWrongLocation
echo.
echo BuildUAT ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
echo.
set RUNUAT_EXITCODE=1
goto Exit

:Error_NoDotnetSDK
echo.
echo RunUBT ERROR: Unable to find a install of Dotnet SDK.  Please make sure you have it installed and that `dotnet` is a globally available command.
echo.
set RUNUAT_EXITCODE=1
goto Exit

:Error_UATCompileFailed
echo.
echo RunUBT ERROR: UnrealBuildTool failed to compile.
echo.
set RUNUAT_EXITCODE=1
goto Exit

:Exit
rem ## Restore original CWD in case we change it
popd
exit /B %RUNUAT_EXITCODE%
