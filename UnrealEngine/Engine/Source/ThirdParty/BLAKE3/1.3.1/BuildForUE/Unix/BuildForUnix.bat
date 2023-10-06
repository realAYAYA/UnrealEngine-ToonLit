@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu -TargetLib=BLAKE3 -TargetLibVersion=1.3.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=BLAKE3 -TargetLibVersion=1.3.1 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -SkipCreateChangelist || exit /b
