@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..

set CMAKE_ADDITIONAL_ARGUMENTS=
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetLib=SentryNative -TargetLibVersion=0.6.6 -TargetConfigs=Release -LibOutputPath=lib-md -CMakeGenerator=VS2022 -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -SkipCreateChangelist || exit /b

set CMAKE_ADDITIONAL_ARGUMENTS=-DSENTRY_BUILD_RUNTIMESTATIC=ON -DSENTRY_BUILD_SHARED_LIBS=OFF
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetLib=SentryNative -TargetLibVersion=0.6.6 -TargetConfigs=Release -LibOutputPath=lib -CMakeGenerator=VS2022 -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS%" -SkipCreateChangelist || exit /b
