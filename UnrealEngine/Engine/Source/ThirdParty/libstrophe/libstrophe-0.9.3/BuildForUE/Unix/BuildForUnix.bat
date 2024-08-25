@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set ENGINE_ROOT=%CD:\=/%/../../../../../../
set TARGET_LIB=libstrophe
set TARGET_LIB_VERSION=libstrophe-0.9.3
set TARGET_PLATFORM=Unix
set TARGET_ARCHITECTURE=x86_64-unknown-linux-gnu
set TARGET_CONFIGS=Release+Debug
set LIB_OUTPUT_PATH=Lib
set UE4_EXPAT_ROOT_DIR=%ENGINE_ROOT%/Source/ThirdParty/Expat/expat-2.2.10
set UE4_OPENSSL_ROOT_DIR=%ENGINE_ROOT%/Source/ThirdParty/OpenSSL/1.1.1t
set CMAKE_GENERATOR=Makefile
set CMAKE_ADDITIONAL_ARGUMENTS="-DBUILD_PIC_LIBRARY:BOOL=ON -DEXPAT_PATH=%UE4_EXPAT_ROOT_DIR%/Lib -DOPENSSL_PATH=%UE4_OPENSSL_ROOT_DIR%/include/Unix"
set MAKE_TARGET=


call "%ENGINE_ROOT%/Build/BatchFiles/RunUAT.bat" BuildCMakeLib -TargetLib=%TARGET_LIB% -TargetLibVersion=%TARGET_LIB_VERSION% -TargetPlatform=%TARGET_PLATFORM% -TargetArchitecture=%TARGET_ARCHITECTURE% -TargetConfigs=%TARGET_CONFIGS% -LibOutputPath=%LIB_OUTPUT_PATH% -CMakeGenerator=%CMAKE_GENERATOR% -CMakeAdditionalArguments=%CMAKE_ADDITIONAL_ARGUMENTS% -MakeTarget=%MAKE_TARGET% -SkipSubmit || exit /b

set TARGET_ARCHITECTURE=aarch64-unknown-linux-gnueabi
call "%ENGINE_ROOT%/Build/BatchFiles/RunUAT.bat" BuildCMakeLib -TargetLib=%TARGET_LIB% -TargetLibVersion=%TARGET_LIB_VERSION% -TargetPlatform=%TARGET_PLATFORM% -TargetArchitecture=%TARGET_ARCHITECTURE% -TargetConfigs=%TARGET_CONFIGS% -LibOutputPath=%LIB_OUTPUT_PATH% -CMakeGenerator=%CMAKE_GENERATOR% -CMakeAdditionalArguments=%CMAKE_ADDITIONAL_ARGUMENTS% -MakeTarget=%MAKE_TARGET% -SkipSubmit || exit /b

goto Exit

:Exit
endlocal
