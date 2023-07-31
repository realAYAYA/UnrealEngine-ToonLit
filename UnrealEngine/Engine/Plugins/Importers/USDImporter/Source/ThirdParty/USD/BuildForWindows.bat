@echo off
setlocal

set USD_VERSION=22.08

rem This path may be adjusted to point to wherever the USD source is located.
rem It is typically obtained by either downloading a zip/tarball of the source
rem code, or more commonly by cloning the GitHub repository, e.g. for the
rem current engine USD version:
rem     git clone --branch v22.08 https://github.com/PixarAnimationStudios/USD.git USD_src
rem Note that a small patch to the USD CMake build is currently necessary for
rem the usdAbc plugin to require and link against Imath instead of OpenEXR:
rem     git apply USD_v2208_usdAbc_Imath.patch
rem We also apply a patch for the usdMtlx plugin to ensure that we do not
rem bake a hard-coded path to the MaterialX standard data libraries into the
rem built plugin:
rem     git apply USD_v2208_usdMtlx_undef_stdlib_dir.patch
rem Note also that this path may be emitted as part of USD error messages, so
rem it is suggested that it not reveal any sensitive information.
set USD_SOURCE_LOCATION=C:\USD_src

rem Set as VS2015 for backwards compatibility even though VS2019 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015
set TOOLCHAIN_NAME=vc14
set ARCH_NAME=x64

set UE_ENGINE_LOCATION=%~dp0\..\..\..\..\..\..

set UE_THIRD_PARTY_LOCATION=%UE_ENGINE_LOCATION%\Source\ThirdParty
set TBB_LOCATION=%UE_THIRD_PARTY_LOCATION%\Intel\TBB\IntelTBB-2019u8
set TBB_INCLUDE_LOCATION=%TBB_LOCATION%\include
set TBB_LIB_LOCATION=%TBB_LOCATION%\lib\Win64\%TOOLCHAIN_NAME%
set BOOST_LOCATION=%UE_THIRD_PARTY_LOCATION%\Boost\boost-1_70_0
set BOOST_INCLUDE_LOCATION=%BOOST_LOCATION%\include
set BOOST_LIB_LOCATION=%BOOST_LOCATION%\lib\Win64
set IMATH_LOCATION=%UE_THIRD_PARTY_LOCATION%\Imath\Deploy\Imath-3.1.3
set IMATH_LIB_LOCATION=%IMATH_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%
set IMATH_CMAKE_LOCATION=%IMATH_LIB_LOCATION%\lib\cmake\Imath
set ALEMBIC_LOCATION=%UE_THIRD_PARTY_LOCATION%\Alembic\Deploy\alembic-1.8.2
set ALEMBIC_INCLUDE_LOCATION=%ALEMBIC_LOCATION%\include
set ALEMBIC_LIB_LOCATION=%ALEMBIC_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%
set MATERIALX_LOCATION=%UE_THIRD_PARTY_LOCATION%\MaterialX\Deploy\MaterialX-1.38.5
set MATERIALX_LIB_LOCATION=%MATERIALX_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib
set MATERIALX_CMAKE_LOCATION=%MATERIALX_LIB_LOCATION%\cmake\MaterialX

set PYTHON_BINARIES_LOCATION=%UE_ENGINE_LOCATION%\Binaries\ThirdParty\Python3\Win64
set PYTHON_EXECUTABLE_LOCATION=%PYTHON_BINARIES_LOCATION%\python.exe
set PYTHON_SOURCE_LOCATION=%UE_THIRD_PARTY_LOCATION%\Python3\Win64
set PYTHON_INCLUDE_LOCATION=%PYTHON_SOURCE_LOCATION%\include
set PYTHON_LIBRARY_LOCATION=%PYTHON_SOURCE_LOCATION%\libs\python39.lib

set UE_MODULE_USD_LOCATION=%~dp0

set BUILD_LOCATION=%UE_MODULE_USD_LOCATION%\Intermediate

rem USD build products are written into a deployment directory and must then
rem be manually copied from there into place.
set INSTALL_LOCATION=%BUILD_LOCATION%\Deploy\USD-%USD_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\include

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

echo Configuring build for USD version %USD_VERSION%...
cmake -G "Visual Studio 16 2019" %USD_SOURCE_LOCATION%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_PREFIX_PATH="%IMATH_CMAKE_LOCATION%;%MATERIALX_CMAKE_LOCATION%"^
    -DTBB_INCLUDE_DIR="%TBB_INCLUDE_LOCATION%"^
    -DTBB_LIBRARY="%TBB_LIB_LOCATION%"^
    -DBoost_NO_BOOST_CMAKE=ON^
    -DBoost_NO_SYSTEM_PATHS=ON^
    -DBOOST_INCLUDEDIR="%BOOST_INCLUDE_LOCATION%"^
    -DBOOST_LIBRARYDIR="%BOOST_LIB_LOCATION%"^
    -DPXR_USE_PYTHON_3=ON^
    -DPYTHON_EXECUTABLE="%PYTHON_EXECUTABLE_LOCATION%"^
    -DPYTHON_INCLUDE_DIR="%PYTHON_INCLUDE_LOCATION%"^
    -DPYTHON_LIBRARY="%PYTHON_LIBRARY_LOCATION%"^
    -DPXR_BUILD_ALEMBIC_PLUGIN=ON^
    -DPXR_ENABLE_HDF5_SUPPORT=OFF^
    -DALEMBIC_INCLUDE_DIR="%ALEMBIC_INCLUDE_LOCATION%"^
    -DALEMBIC_DIR="%ALEMBIC_LIB_LOCATION%"^
    -DPXR_ENABLE_MATERIALX_SUPPORT=ON^
    -DBUILD_SHARED_LIBS=ON^
    -DPXR_BUILD_TESTS=OFF^
    -DPXR_BUILD_EXAMPLES=OFF^
    -DPXR_BUILD_TUTORIALS=OFF^
    -DPXR_BUILD_USD_TOOLS=OFF^
    -DPXR_BUILD_IMAGING=OFF^
    -DPXR_BUILD_USD_IMAGING=OFF^
    -DPXR_BUILD_USDVIEW=OFF^
    -DCMAKE_CXX_FLAGS="/Zm150"
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building USD for Release...
cmake --build . --config Release -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing USD for Release...
cmake --install . --config Release
if %errorlevel% neq 0 exit /B %errorlevel%

popd

echo Moving shared libraries to bin directory...
set INSTALL_BIN_LOCATION=%INSTALL_LOCATION%\bin
set INSTALL_LIB_LOCATION=%INSTALL_LOCATION%\lib
mkdir %INSTALL_BIN_LOCATION%
move "%INSTALL_LIB_LOCATION%\*.dll" "%INSTALL_BIN_LOCATION%"

echo Moving built-in USD plugins to UsdResources plugins directory...
set INSTALL_RESOURCES_LOCATION=%INSTALL_LOCATION%\Resources\UsdResources\Win64
set INSTALL_RESOURCES_PLUGINS_LOCATION=%INSTALL_RESOURCES_LOCATION%\plugins
mkdir %INSTALL_RESOURCES_LOCATION%
move "%INSTALL_LIB_LOCATION%\usd" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"

echo Moving USD plugin shared libraries to bin directory...
set INSTALL_PLUGIN_LOCATION=%INSTALL_LOCATION%\plugin
set INSTALL_PLUGIN_USD_LOCATION=%INSTALL_PLUGIN_LOCATION%\usd
move "%INSTALL_PLUGIN_USD_LOCATION%\*.dll" "%INSTALL_BIN_LOCATION%"

echo Moving USD plugin import libraries to lib directory...
move "%INSTALL_PLUGIN_USD_LOCATION%\*.lib" "%INSTALL_LIB_LOCATION%"

echo Removing top-level USD plugins plugInfo.json file...
del "%INSTALL_PLUGIN_USD_LOCATION%\plugInfo.json"

echo Moving UsdAbc plugin directory to UsdResources plugins directory
move "%INSTALL_PLUGIN_USD_LOCATION%\usdAbc" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"

rmdir "%INSTALL_PLUGIN_USD_LOCATION%"
rmdir "%INSTALL_PLUGIN_LOCATION%"

echo Removing CMake files...
rmdir /S /Q "%INSTALL_LOCATION%\cmake"
del /S /Q "%INSTALL_LOCATION%\*.cmake"

echo Removing Python .pyc files...
del /S /Q "%INSTALL_LOCATION%\*.pyc"

echo Removing pxr.Tf.testenv Python module...
rmdir /S /Q "%INSTALL_LOCATION%\lib\python\pxr\Tf\testenv"

echo Moving Python modules to Content
set INSTALL_CONTENT_LOCATION=%INSTALL_LOCATION%\Content\Python\Lib\Win64\site-packages
mkdir %INSTALL_CONTENT_LOCATION%
move "%INSTALL_LOCATION%\lib\python\pxr" "%INSTALL_CONTENT_LOCATION%"
rmdir "%INSTALL_LOCATION%\lib\python"

echo Done.

endlocal
