@rem Copyright Epic Games, Inc. All Rights Reserved.
@echo off

set ENGINE_ROOT=%~dp0..\..\..\..\..\..
set THIRDPARTY_ROOT=%ENGINE_ROOT%\Source\ThirdParty
set CMAKE_ADDITIONAL_ARGUMENTS=-DBUILD_SHARED_LIBS=OFF -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF
set CMAKE_ZLIB_ARGUMENTS=-DZLIB_ROOT=%THIRDPARTY_ROOT%\zlib\1.2.12\lib\Win64\Release -DZLIB_INCLUDE_DIR=%THIRDPARTY_ROOT%\zlib\1.2.12\include
call "%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat" BuildCMakeLib -TargetPlatform=Win64 -TargetArchitecture=x64 -TargetLib=Protobuf -TargetLibVersion=21.1 -TargetConfigs=Release -LibOutputPath=lib -BinOutputPath=bin -IncludeOutputPath=include -CMakeGenerator=VS2019 -CMakeAdditionalArguments="%CMAKE_ADDITIONAL_ARGUMENTS% %CMAKE_ZLIB_ARGUMENTS%" -SkipCreateChangelist || exit /b
