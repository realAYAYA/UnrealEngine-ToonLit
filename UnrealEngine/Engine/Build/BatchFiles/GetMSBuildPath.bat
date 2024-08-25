@echo off

rem ## Unreal Engine utility script
rem ## Copyright Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script determines the path to MSBuild necessary to compile C# tools for the current version of the engine.
rem ## The discovered path is set to the MSBUILD_EXE environment variable on success.

set MSBUILD_EXE=

rem ## Try to get the MSBuild path using vswhere (see https://github.com/Microsoft/vswhere)
if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" goto no_vswhere
for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -prerelease -latest -products * -requires Microsoft.Component.MSBuild -property installationPath') do (
	if exist "%%i\MSBuild\Current\Bin\MSBuild.exe" (
		set MSBUILD_EXE="%%i\MSBuild\Current\Bin\MSBuild.exe"
		goto Succeeded
	)
)
:no_vswhere

rem ## Couldn't find anything
exit /B 1

rem ## Did manage to locate MSBuild
:Succeeded
exit /B 0
