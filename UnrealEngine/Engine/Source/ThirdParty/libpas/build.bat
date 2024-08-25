@rem Copyright Epic Games, Inc. All Rights Reserved.

@echo off
setlocal enabledelayedexpansion

p4 edit x64\DebugUE\libpas.lib x64\ReleaseUE\libpas.lib
if %errorlevel% neq 0 exit /b 1

msbuild libpas.sln -p:Configuration=Debug
if %errorlevel% neq 0 exit /b 1
msbuild libpas.sln -p:Configuration=ReleaseTesting
if %errorlevel% neq 0 exit /b 1
msbuild libpas.vcxproj -p:Configuration=DebugUE -p:Platform=x64
if %errorlevel% neq 0 exit /b 1
msbuild libpas.vcxproj -p:Configuration=ReleaseUE -p:Platform=x64
if %errorlevel% neq 0 exit /b 1

echo SUCCESS

