@echo off
setlocal

set ALEMBIC_VERSION=1.8.2

rem Set as VS2015 for backwards compatibility even though VS2019 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015
set ARCH_NAME=x64

set UE_THIRD_PARTY_LOCATION=%cd%\..
set IMATH_CMAKE_LOCATION=%UE_THIRD_PARTY_LOCATION%\Imath\Deploy\Imath-3.1.3\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib\cmake\Imath

set UE_MODULE_LOCATION=%cd%

set SOURCE_LOCATION=%UE_MODULE_LOCATION%\alembic-%ALEMBIC_VERSION%

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\Deploy\alembic-%ALEMBIC_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\include
set INSTALL_WIN_LOCATION=%INSTALL_LOCATION%\%COMPILER_VERSION_NAME%
set INSTALL_LIB_DIR=%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib
rem The Alembic build is setup incorrectly such that relative install paths
rem land in the build tree rather than the install tree. To make sure the
rem library is installed in the correct location, we use a full path. Doing so
rem prevents CMake from computing the correct import prefix though, so the
rem resulting config files include absolute paths that we don't want. We won't
rem really miss having these CMake files since we are unlikely to build
rem anything on top of Alembic using CMake, so we use a relative path for those
rem and let them disappear when the build tree in "Intermediate" is cleaned.
set INSTALL_LIB_LOCATION=%INSTALL_LOCATION%\%INSTALL_LIB_DIR%
set INSTALL_CMAKE_DIR=%INSTALL_LIB_DIR%\cmake\Alembic

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)
if exist %INSTALL_INCLUDE_LOCATION% (
    rmdir %INSTALL_INCLUDE_LOCATION% /S /Q)
if exist %INSTALL_WIN_LOCATION% (
    rmdir %INSTALL_WIN_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

echo Configuring build for Alembic version %ALEMBIC_VERSION%...
cmake -G "Visual Studio 16 2019" %SOURCE_LOCATION%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_PREFIX_PATH="%IMATH_CMAKE_LOCATION%"^
    -DALEMBIC_LIB_INSTALL_DIR="%INSTALL_LIB_LOCATION%"^
    -DConfigPackageLocation="%INSTALL_CMAKE_DIR%"^
    -DUSE_BINARIES=OFF^
    -DUSE_TESTS=OFF^
    -DALEMBIC_ILMBASE_LINK_STATIC=ON^
    -DALEMBIC_SHARED_LIBS=OFF^
    -DCMAKE_DEBUG_POSTFIX=_d
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building Alembic for Debug...
cmake --build . --config Debug -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing Alembic for Debug...
cmake --install . --config Debug
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building Alembic for Release...
cmake --build . --config Release -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing Alembic for Release...
cmake --install . --config Release
if %errorlevel% neq 0 exit /B %errorlevel%

popd

echo Done.

endlocal
