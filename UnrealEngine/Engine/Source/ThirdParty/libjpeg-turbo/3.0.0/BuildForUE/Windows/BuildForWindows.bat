@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetLib=libjpeg-turbo -TargetLibVersion=3.0.0 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2019 -MakeTarget=turbojpeg-static -CMakeAdditionalArguments="-DREQUIRE_SIMD=ON" -SkipCreateChangelist || exit /b

mkdir %~dp0..\..\include
copy /y %~dp0..\..\turbojpeg.h %~dp0..\..\include\
