@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

rem SET UE4_OPENSSL_ROOT_DIR="D:/YOUR_UE4_WORKSPACE_HERE/Engine/Source/ThirdParty/OpenSSL/1.1.1c"
rem SET UE4_EXPAT_ROOT_DIR="D:/YOUR_UE4_WORKSPACE_HERE/Engine/Source/ThirdParty/Expat/expat-2.2.10"

REM This should match to keep everyone sane!
SET UE_TOOLSET=v140
SET UE_TOOLSET_IDE=VS2015

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
REM (UE4 needs VS2015 folder, even for VS2017/VS2019 builds)
set VS2015_X64_PATH="%PATH_TO_CMAKE_FILE%..\Win64\%UE_TOOLSET_IDE%\Build"
set WIN64_LIB_PATH="%PATH_TO_CMAKE_FILE%\..\Lib\Win64\%UE_TOOLSET_IDE%"

REM UE4 third-party folder paths
if not exist "%UE4_OPENSSL_ROOT_DIR%" goto OpenSSLMissing
if not exist "%UE4_EXPAT_ROOT_DIR%" goto ExpatMissing

REM MSBuild Directory
for /f "delims=" %%V in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath') do SET _vsinstall=%%V
if errorlevel 1 goto VStudioMissing
SET _msbuild=%_vsinstall%\MSBuild\Current\Bin\
if not exist "%_msbuild%msbuild.exe" goto MSBuildMissing

REM Build for VS2017 (64-bit)
echo Generating libstrophe solution for VS2017 (64-bit)...
if exist "%VS2015_X64_PATH%" (rmdir "%VS2015_X64_PATH%" /s/q)
mkdir "%VS2015_X64_PATH%"
cd "%VS2015_X64_PATH%"
"%_vsinstall%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -G "Visual Studio 17 2022" -A x64 -T %UE_TOOLSET% -DCMAKE_SUPPRESS_REGENERATION=1 -DSOCKET_IMPL=../../../src/sock.c -DOPENSSL_PATH="%UE4_OPENSSL_ROOT_DIR%/include/Win64/%UE_TOOLSET_IDE%" -DDISABLE_TLS=0 -DEXPAT_PATH="%UE4_EXPAT_ROOT_DIR%/lib" %PATH_TO_CMAKE_FILE%
echo Building libstrophe solution for VS2017 (64-bit, Debug)...
"%_msbuild%msbuild.exe" libstrophe.sln /t:build /p:Configuration=Debug
echo Building libstrophe solution for VS2017 (64-bit, Release)...
"%_msbuild%msbuild.exe" libstrophe.sln /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
move /y %VS2015_X64_PATH%\Debug\strophe.lib %WIN64_LIB_PATH%\Debug
move /y %VS2015_X64_PATH%\Debug\strophe.pdb %WIN64_LIB_PATH%\Debug
move /y %VS2015_X64_PATH%\Release\strophe.lib %WIN64_LIB_PATH%\Release
echo rmdir "%VS2015_X64_PATH%" /s/q
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
