@echo off
setlocal

rem Copyright Epic Games, Inc. All Rights Reserved.

if [%1]==[] goto usage
if [%2]==[] goto usage

set OPENSUBDIV_VERSION=%1

rem Set as VS2015 for backwards compatibility even though VS2022 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015
set ARCH_NAME=%2

set BUILD_SCRIPT_LOCATION=%~dp0

set UE_MODULE_LOCATION=%BUILD_SCRIPT_LOCATION%..\..

set SOURCE_LOCATION=%UE_MODULE_LOCATION%\OpenSubdiv-%OPENSUBDIV_VERSION%

set UE_ENGINE_LOCATION=%UE_MODULE_LOCATION%\..\..\..

rem To simplify cross-compilation, we provide the path to the engine's Python
rem interpreter so that we use OpenSubdiv's Python version of the stringify
rem tool, preventing it from building the C++ version.
set PYTHON_BINARIES_LOCATION=%UE_ENGINE_LOCATION%\Binaries\ThirdParty\Python3\Win64
set PYTHON_EXECUTABLE_LOCATION=%PYTHON_BINARIES_LOCATION%\python.exe

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

rem Specify all of the include/bin/lib directory variables so that CMake can
rem compute relative paths correctly for the imported targets.
set INSTALL_INCLUDEDIR=include
set INSTALL_BIN_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\bin
set INSTALL_LIB_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\OpenSubdiv-%OPENSUBDIV_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\%INSTALL_INCLUDEDIR%
set INSTALL_WIN_LOCATION=%INSTALL_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)
if exist %INSTALL_INCLUDE_LOCATION% (
    rmdir %INSTALL_INCLUDE_LOCATION% /S /Q)
if exist %INSTALL_WIN_LOCATION% (
    rmdir %INSTALL_WIN_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

echo Configuring build for OpenSubdiv version %OPENSUBDIV_VERSION%...
cmake -G "Visual Studio 17 2022" %SOURCE_LOCATION%^
    -A %ARCH_NAME%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_BINDIR_BASE="%INSTALL_BIN_DIR%"^
    -DCMAKE_LIBDIR_BASE="%INSTALL_LIB_DIR%"^
    -DCMAKE_INSTALL_LIBDIR="%INSTALL_LIB_DIR%"^
    -DPython_EXECUTABLE="%PYTHON_EXECUTABLE_LOCATION%"^
    -DCMAKE_DEBUG_POSTFIX=_d^
    -DNO_REGRESSION=ON^
    -DNO_TESTS=ON^
    -DNO_DOC=ON^
    -DNO_EXAMPLES=ON^
    -DNO_TUTORIALS=ON^
    -DNO_PTEX=ON^
    -DNO_TBB=ON^
    -DNO_OMP=ON^
    -DNO_CUDA=ON^
    -DNO_OPENCL=ON^
    -DNO_DX=ON^
    -DNO_GLEW=ON^
    -DNO_GLFW=ON^
    -DBUILD_SHARED_LIBS=OFF
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building OpenSubdiv for Debug...
cmake --build . --config Debug -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing OpenSubdiv for Debug...
cmake --install . --config Debug
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building OpenSubdiv for Release...
cmake --build . --config Release -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing OpenSubdiv for Release...
cmake --install . --config Release
if %errorlevel% neq 0 exit /B %errorlevel%

popd

rem Remove the installed bin dir. The only thing in there will be stringify.exe
rem which we don't need.
echo Removing unused executables in bin directory...
if exist "%INSTALL_LOCATION%\%INSTALL_BIN_DIR%" (
    rmdir "%INSTALL_LOCATION%\%INSTALL_BIN_DIR%" /S /Q)

echo Done.

goto :eof

:usage
echo Usage: BuildForWindows ^<version^> ^<architecture: x64 or ARM64^>
exit /B 1

endlocal
