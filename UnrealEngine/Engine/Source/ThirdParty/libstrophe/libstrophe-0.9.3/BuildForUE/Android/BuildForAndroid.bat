@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set ANDROID_NDK_ROOT=%NDKROOT%

set ENGINE_ROOT=%CD:\=/%/../../../../../../
set TARGET_LIB=libstrophe
set TARGET_LIB_VERSION=libstrophe-0.9.3
set TARGET_PLATFORM=Android
set TARGET_ARCHITECTURE=arm64
set TARGET_CONFIGS=Release+Debug
set LIB_OUTPUT_PATH=Lib
set UE4_EXPAT_ROOT_DIR=%ENGINE_ROOT%/Source/ThirdParty/Expat/expat-2.2.10
set UE4_OPENSSL_ROOT_DIR=%ENGINE_ROOT%/Source/ThirdParty/OpenSSL/1.1.1t
set CMAKE_GENERATOR=Makefile
set CMAKE_ADDITIONAL_ARGUMENTS="-DEXPAT_PATH=%UE4_EXPAT_ROOT_DIR%/Lib -DOPENSSL_PATH=%UE4_OPENSSL_ROOT_DIR%/include/Android"
set MAKE_TARGET=


call "%ENGINE_ROOT%/Build/BatchFiles/RunUAT.bat" BuildCMakeLib -TargetLib=%TARGET_LIB% -TargetLibVersion=%TARGET_LIB_VERSION% -TargetPlatform=%TARGET_PLATFORM% -TargetArchitecture=%TARGET_ARCHITECTURE% -TargetConfigs=%TARGET_CONFIGS% -LibOutputPath=%LIB_OUTPUT_PATH% -CMakeGenerator=%CMAKE_GENERATOR% -CMakeAdditionalArguments=%CMAKE_ADDITIONAL_ARGUMENTS% -MakeTarget=%MAKE_TARGET% -SkipSubmit || exit /b


set TARGET_ARCHITECTURE=x64
call "%ENGINE_ROOT%/Build/BatchFiles/RunUAT.bat" BuildCMakeLib -TargetLib=%TARGET_LIB% -TargetLibVersion=%TARGET_LIB_VERSION% -TargetPlatform=%TARGET_PLATFORM% -TargetArchitecture=%TARGET_ARCHITECTURE% -TargetConfigs=%TARGET_CONFIGS% -LibOutputPath=%LIB_OUTPUT_PATH% -CMakeGenerator=%CMAKE_GENERATOR% -CMakeAdditionalArguments=%CMAKE_ADDITIONAL_ARGUMENTS% -MakeTarget=%MAKE_TARGET% -SkipSubmit || exit /b

goto Exit

:ExpatMissing
echo Could not find UE4 Expat. Please check your UE4_EXPAT_ROOT_DIR environment variable and try again.
goto Exit

:NDKRootMissing
echo Could not find Android NDK Root. Please check your NDKROOT environment variable and try again.
goto Exit

:Exit
endlocal
