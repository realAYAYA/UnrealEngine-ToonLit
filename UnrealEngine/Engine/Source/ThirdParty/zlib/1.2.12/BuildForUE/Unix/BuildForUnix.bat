@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu -TargetLib=zlib -TargetLibVersion=1.2.12 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -MakeTarget=zlibstatic -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=zlib -TargetLibVersion=1.2.12 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -MakeTarget=zlibstatic -SkipCreateChangelist || exit /b
