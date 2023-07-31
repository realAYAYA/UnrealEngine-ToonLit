@ECHO OFF

REM Copyright Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\..

REM This should match to keep everyone sane!
SET UE_TOOLSET=v140
SET UE_TOOLSET_IDE=VS2015

REM Temporary build directories (used as working directories when running CMake)
set UE_BUILD_PATH="%PATH_TO_CMAKE_FILE%\Win64\%UE_TOOLSET_IDE%\Build"

REM MSBuild Directory
for /f "delims=" %%V in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath') do SET _vsinstall=%%V
if errorlevel 1 goto VStudioMissing
SET _msbuild=%_vsinstall%\MSBuild\Current\Bin\
if not exist "%_msbuild%msbuild.exe" goto MSBuildMissing

REM Build for 64-bit
echo Generating Expat solution (64-bit)...
if exist "%UE_BUILD_PATH%" (rmdir "%UE_BUILD_PATH%" /s/q)
mkdir "%UE_BUILD_PATH%"
cd "%UE_BUILD_PATH%"
"%_vsinstall%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -G "Visual Studio 17 2022" -A x64 -T %UE_TOOLSET% -DEXPAT_BUILD_TOOLS=0 -DEXPAT_BUILD_EXAMPLES=0 -DEXPAT_BUILD_TESTS=0 -DEXPAT_SHARED_LIBS=0 %PATH_TO_CMAKE_FILE%
echo Building Expat solution for (64-bit, Debug)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Debug
echo Building Expat solution for (64-bit, Release)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
xcopy /y/s/i "%UE_BUILD_PATH%\Debug" "%UE_BUILD_PATH%\..\Debug"
xcopy /y/s/i "%UE_BUILD_PATH%\Release" "%UE_BUILD_PATH%\..\Release"
rmdir "%UE_BUILD_PATH%" /s/q
exit /b 0

:VStudioMissing
echo Visual Studio not found. Please check your Visual Studio install and try again.
goto Exit

:MSBuildMissing
echo MSBuild not found. Please check your Visual Studio install and try again.
goto Exit

:Exit
endlocal
exit /b 1