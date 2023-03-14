@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

rem SET UE4_OPENSSL_ROOT_DIR="D:/YOUR_UE4_WORKSPACE_HERE/Engine/Source/ThirdParty/OpenSSL/1.1.1c"
rem SET UE4_EXPAT_ROOT_DIR="D:/YOUR_UE4_WORKSPACE_HERE/Engine/Source/ThirdParty/Expat/expat-2.2.10"

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
REM (UE4 needs VS2015 folder, even for VS2017/VS2019 builds)
set VS2015_X86_PATH="%PATH_TO_CMAKE_FILE%..\Win32\VS2015\Build"
set VS2015_X64_PATH="%PATH_TO_CMAKE_FILE%..\Win64\VS2015\Build"

REM UE4 third-party folder paths
if not exist "%UE4_OPENSSL_ROOT_DIR%" goto OpenSSLMissing
if not exist "%UE4_EXPAT_ROOT_DIR%" goto ExpatMissing

REM MSBuild Directory
SET _msbuild="C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\MSBuild\\15.0\\Bin"
if not exist %_msbuild%\\msbuild.exe SET _msbuild="C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\MSBuild\\15.0\\Bin"
if not exist %_msbuild%\\msbuild.exe SET _msbuild="C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Exterprise\\MSBuild\\15.0\\Bin"
if not exist %_msbuild%\\msbuild.exe goto MSBuildMissing

REM Build for VS2017 (32-bit)
echo Generating libstrophe solution for VS2017 (32-bit)...
if exist "%VS2015_X86_PATH%" (rmdir "%VS2015_X86_PATH%" /s/q)
mkdir "%VS2015_X86_PATH%"
cd "%VS2015_X86_PATH%"
cmake -G "Visual Studio 15 2017" -DCMAKE_SUPPRESS_REGENERATION=1 -DSOCKET_IMPL=../../../src/sock.c -DOPENSSL_PATH="%UE4_OPENSSL_ROOT_DIR%/include/Win32" -DDISABLE_TLS=0 -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%/lib" %PATH_TO_CMAKE_FILE%
echo Building libstrophe solution for VS2017 (32-bit, Debug)...
%_msbuild%\\msbuild.exe libstrophe.sln /t:build /p:Configuration=Debug
echo Building libstrophe solution for VS2017 (32-bit, Release)...
%_msbuild%\\msbuild.exe libstrophe.sln /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
rmdir "%VS2015_X86_PATH%" /s/q

REM Build for VS2017 (64-bit)
echo Generating libstrophe solution for VS2017 (64-bit)...
if exist "%VS2015_X64_PATH%" (rmdir "%VS2015_X64_PATH%" /s/q)
mkdir "%VS2015_X64_PATH%"
cd "%VS2015_X64_PATH%"
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_SUPPRESS_REGENERATION=1 -DSOCKET_IMPL=../../../src/sock.c -DOPENSSL_PATH="%UE4_OPENSSL_ROOT_DIR%/include/Win32" -DDISABLE_TLS=0 -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%/lib" %PATH_TO_CMAKE_FILE%
echo Building libstrophe solution for VS2017 (64-bit, Debug)...
%_msbuild%\\msbuild.exe libstrophe.sln /t:build /p:Configuration=Debug
echo Building libstrophe solution for VS2017 (64-bit, Release)...
%_msbuild%\\msbuild.exe libstrophe.sln /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
rmdir "%VS2015_X64_PATH%" /s/q
goto Exit

:MSBuildMissing
echo MSBuild not found. Please check your Visual Studio install and try again.
goto Exit

:OpenSSLMissing
echo Could not find UE4 Openssl. Please check your UE4_OPENSSL_ROOT_DIR environment variable and try again.
goto Exit

:ExpatMissing
echo Could not find UE4 Expat. Please check your UE4_EXPAT_ROOT_DIR environment variable and try again.
goto Exit

:Exit
endlocal
