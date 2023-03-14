@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetLib=zlib -TargetLibVersion=1.2.12 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2019 -CMakeAdditionalArguments="-DMINIZIP=ON" -MakeTarget=zlibstatic -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetArchitecture=ARM64 -TargetLib=zlib -TargetLibVersion=1.2.12 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2019 -MakeTarget=zlibstatic -SkipCreateChangelist || exit /b
