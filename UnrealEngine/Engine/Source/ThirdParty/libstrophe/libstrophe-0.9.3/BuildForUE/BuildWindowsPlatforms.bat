@echo off

REM Copyright Epic Games, Inc. All Rights Reserved.

rem It is recommended to have the latest CMake 3.10+ installed for VS2017 support

setlocal

REM Set your UE4 root dir here (The folder that has Engine in it)
set UE4_ENGINE_ROOT_DIR=D:/your/ue4/depot

REM If you don't have your NDKROOT set on your environment variable, fill-in the path to your CarefullyRedist
if not exist "%NDKROOT%" set NDKROOT=D:/your/carefully/redist/depot/CarefullyRedist/HostWin64/Android/-22/android-ndk-r14b

REM You many need to change some of these versions over time
set UE4_OPENSSL_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Source/ThirdParty/OpenSSL/1.1.1c
set UE4_EXPAT_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Source/ThirdParty/Expat/expat-2.2.10
set UE4_CMAKE_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Source/ThirdParty/CMake
set UE4_LWS_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Source/ThirdParty/libWebSockets/libwebsockets
set UE4_GNUMAKE_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Extras/ThirdPartyNotUE/GNU_Make
@REM It's a bit ridiculous, but we use physx's android cmake file to build Android!
set UE4_PHYSX_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Source/ThirdParty/PhysX3

REM Actually build the platforms
@echo Building Windows
cd %cd%/Windows
call BuildForWindows.bat
cd ..

@echo NDA'd platforms have been removed from these build scripts, but they can be called here if you have been granted access to those platforms

REM Android isn't ready for OpenSSL 1.1.1c yet, remove this when it is and can match the global version
set UE4_OPENSSL_ROOT_DIR=%UE4_ENGINE_ROOT_DIR%/Engine/Source/ThirdParty/OpenSSL/1_0_1s
@echo Building Android
cd %cd%/Android
call BuildForAndroid.bat
cd ..

endlocal

pause
