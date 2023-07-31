@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
set CMAKE_ADDITIONAL_ARGUMENTS=-DRE2_BUILD_TESTING=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetArchitecture=x64 -TargetLib=re2 -TargetLibVersion=2022-06-01 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2019 -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -SkipCreateChangelist || exit /b
