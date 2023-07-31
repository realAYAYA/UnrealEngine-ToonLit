@echo off

rem ## Unreal Engine code rebuild script
rem ## Copyright Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script is expecting to exist in the Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.
rem ##
rem ##     %1 is the game name
rem ##     %2 is the platform name
rem ##     %3 is the configuration name
rem ##     additional args are passed directly to UnrealBuildTool

IF NOT EXIST "%~dp0\Build.bat" GOTO Error_MissingBuildBatchFile

call "%~dp0\Build.bat" %* -Rebuild
goto Exit

:Error_MissingBuildBatchFile
ECHO Build.bat not found in "%~dp0"
EXIT /B 999

:Exit
