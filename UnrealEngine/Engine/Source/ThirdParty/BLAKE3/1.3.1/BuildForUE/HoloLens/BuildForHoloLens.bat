@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=HoloLens -TargetArchitecture=x64 -TargetLib=BLAKE3 -TargetLibVersion=1.3.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2019 -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=HoloLens -TargetArchitecture=ARM64 -TargetLib=BLAKE3 -TargetLibVersion=1.3.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2019 -SkipCreateChangelist || exit /b
