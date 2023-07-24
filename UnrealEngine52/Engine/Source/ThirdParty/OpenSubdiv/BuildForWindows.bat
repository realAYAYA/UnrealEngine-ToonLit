@echo off
setlocal

set OPENSUBDIV_VERSION=3.4.4

rem Set as VS2015 for backwards compatibility even though VS2019 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015
set ARCH_NAME=x64

set UE_MODULE_LOCATION=%cd%

set SOURCE_LOCATION=%UE_MODULE_LOCATION%\OpenSubdiv-%OPENSUBDIV_VERSION%

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

rem Specify all of the include/bin/lib directory variables so that CMake can
rem compute relative paths correctly for the imported targets.
set INSTALL_INCLUDEDIR=include
set INSTALL_BIN_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\bin
set INSTALL_LIB_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\OpenSubdiv-%OPENSUBDIV_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\%INSTALL_INCLUDEDIR%
set INSTALL_INCLUDE_OPENSUBDIV_LOCATION=%INSTALL_INCLUDE_LOCATION%\opensubdiv
set INSTALL_WIN_LOCATION=%INSTALL_LOCATION%\%COMPILER_VERSION_NAME%

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)
if exist %INSTALL_INCLUDE_LOCATION% (
    rmdir %INSTALL_INCLUDE_LOCATION% /S /Q)
if exist %INSTALL_WIN_LOCATION% (
    rmdir %INSTALL_WIN_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

echo Configuring build for OpenSubdiv version %OPENSUBDIV_VERSION%...
cmake -G "Visual Studio 16 2019" %SOURCE_LOCATION%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_INCDIR_BASE="%INSTALL_INCLUDE_OPENSUBDIV_LOCATION%"^
    -DCMAKE_BINDIR_BASE="%INSTALL_BIN_DIR%"^
    -DCMAKE_LIBDIR_BASE="%INSTALL_LIB_DIR%"^
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
    -DNO_GLFW=ON
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

endlocal
