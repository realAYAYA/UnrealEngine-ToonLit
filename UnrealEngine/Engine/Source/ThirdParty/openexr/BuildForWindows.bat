@echo off
setlocal

set OPENEXR_VERSION=3.1.5

rem Set as VS2015 for backwards compatibility even though VS2019 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015
set ARCH_NAME=x64

set UE_THIRD_PARTY_LOCATION=%cd%\..
set IMATH_CMAKE_LOCATION=%UE_THIRD_PARTY_LOCATION%\Imath\Deploy\Imath-3.1.3\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib\cmake\Imath
set ZLIB_LOCATION=%UE_THIRD_PARTY_LOCATION%\zlib\v1.2.8
set ZLIB_INCLUDE_LOCATION=%ZLIB_LOCATION%\include\Win64\%COMPILER_VERSION_NAME%
set ZLIB_LIB_LOCATION=%ZLIB_LOCATION%\lib\Win64\%COMPILER_VERSION_NAME%\Release

set UE_MODULE_LOCATION=%cd%

set SOURCE_LOCATION=%UE_MODULE_LOCATION%\openexr-%OPENEXR_VERSION%

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

rem Specify all of the include/bin/lib directory variables so that CMake can
rem compute relative paths correctly for the imported targets.
set INSTALL_INCLUDEDIR=include
set INSTALL_BIN_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\bin
set INSTALL_LIB_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\openexr-%OPENEXR_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\%INSTALL_INCLUDEDIR%
set INSTALL_WIN_LOCATION=%INSTALL_LOCATION%\%COMPILER_VERSION_NAME%

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)
if exist %INSTALL_INCLUDE_LOCATION% (
    rmdir %INSTALL_INCLUDE_LOCATION% /S /Q)
if exist %INSTALL_WIN_LOCATION% (
    rmdir %INSTALL_WIN_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

echo Configuring build for OpenEXR version %OPENEXR_VERSION%...
cmake -G "Visual Studio 16 2019" %SOURCE_LOCATION%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_PREFIX_PATH="%IMATH_CMAKE_LOCATION%"^
    -DZLIB_INCLUDE_DIR="%ZLIB_INCLUDE_LOCATION%"^
    -DZLIB_ROOT="%ZLIB_LIB_LOCATION%"^
    -DCMAKE_INSTALL_INCLUDEDIR="%INSTALL_INCLUDEDIR%"^
    -DCMAKE_INSTALL_BINDIR="%INSTALL_BIN_DIR%"^
    -DCMAKE_INSTALL_LIBDIR="%INSTALL_LIB_DIR%"^
    -DBUILD_SHARED_LIBS=OFF^
    -DOPENEXR_BUILD_TOOLS=OFF^
    -DBUILD_TESTING=OFF^
    -DOPENEXR_INSTALL_EXAMPLES=OFF^
    -DOPENEXR_INSTALL_PKG_CONFIG=OFF
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building OpenEXR for Debug...
cmake --build . --config Debug -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing OpenEXR for Debug...
cmake --install . --config Debug
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building OpenEXR for Release...
cmake --build . --config Release -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing OpenEXR for Release...
cmake --install . --config Release
if %errorlevel% neq 0 exit /B %errorlevel%

popd

echo Done.

endlocal
