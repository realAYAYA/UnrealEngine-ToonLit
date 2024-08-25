@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=x86_64-unknown-linux-gnu      -TargetLib=libjpeg-turbo -TargetLibVersion=3.0.0 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -MakeTarget=turbojpeg-static -CMakeAdditionalArguments="-DREQUIRE_SIMD=ON" -SkipCreateChangelist || exit /b
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Unix -TargetArchitecture=aarch64-unknown-linux-gnueabi -TargetLib=libjpeg-turbo -TargetLibVersion=3.0.0 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=Makefile -MakeTarget=turbojpeg-static -CMakeAdditionalArguments="-DREQUIRE_SIMD=ON" -SkipCreateChangelist || exit /b
