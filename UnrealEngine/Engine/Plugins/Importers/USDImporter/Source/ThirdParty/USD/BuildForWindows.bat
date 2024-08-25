@echo off
setlocal

set OPENUSD_VERSION=24.03

rem This path may be adjusted to point to wherever the OpenUSD source is
rem located. It is typically obtained by either downloading a zip/tarball of
rem the source code, or more commonly by cloning the GitHub repository, e.g.
rem for the current engine OpenUSD version:
rem     git clone --branch v24.03 https://github.com/PixarAnimationStudios/OpenUSD.git OpenUSD_src
rem We apply a patch for the usdMtlx plugin to ensure that we do not
rem bake a hard-coded path to the MaterialX standard data libraries into the
rem built plugin:
rem     git apply OpenUSD_v2403_usdMtlx_undef_stdlib_dir.patch
rem We apply a patch to explicitly declare, define, and export a destructor for
rem SdfAssetPaths so that allocations of its member strings can be tracked and
rem deallocated using the correct deallocator:
rem     git apply OpenUSD_v2403_explicit_SdfAssetPath_dtor.patch
rem We apply a patch to switch between two alternative set of macros in the Tf
rem library based on whether we're compiling with MSVC *and* whether its
rem "traditional" preprocessor is being used, not just whether we're using
rem MSVC or not:
rem     git apply OpenUSD_v2403_msvc_preprocessor_version_handling.patch
rem Note also that this path may be emitted as part of OpenUSD error messages,
rem so it is suggested that it not reveal any sensitive information.
set OPENUSD_SOURCE_LOCATION=C:\OpenUSD_src

rem Set as VS2015 for backwards compatibility even though VS2022 is used
rem when building.
set COMPILER_VERSION_NAME=VS2015
set TOOLCHAIN_NAME=vc14
set ARCH_NAME=x64

set UE_ENGINE_LOCATION=%~dp0\..\..\..\..\..\..

set UE_THIRD_PARTY_LOCATION=%UE_ENGINE_LOCATION%\Source\ThirdParty
set TBB_LOCATION=%UE_THIRD_PARTY_LOCATION%\Intel\TBB\IntelTBB-2019u8
set TBB_INCLUDE_LOCATION=%TBB_LOCATION%\include
set TBB_LIB_LOCATION=%TBB_LOCATION%\lib\Win64\%TOOLCHAIN_NAME%
set BOOST_LOCATION=%UE_THIRD_PARTY_LOCATION%\Boost\boost-1_82_0
set BOOST_INCLUDE_LOCATION=%BOOST_LOCATION%\include
set BOOST_LIB_LOCATION=%BOOST_LOCATION%\lib\Win64
set IMATH_LOCATION=%UE_THIRD_PARTY_LOCATION%\Imath\Deploy\Imath-3.1.9
set IMATH_LIB_LOCATION=%IMATH_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib
set IMATH_CMAKE_LOCATION=%IMATH_LIB_LOCATION%\cmake\Imath
set OPENSUBDIV_LOCATION=%UE_THIRD_PARTY_LOCATION%\OpenSubdiv\Deploy\OpenSubdiv-3.6.0
set OPENSUBDIV_INCLUDE_DIR=%OPENSUBDIV_LOCATION%\include
set OPENSUBDIV_LIB_LOCATION=%OPENSUBDIV_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib
set ALEMBIC_LOCATION=%UE_THIRD_PARTY_LOCATION%\Alembic\Deploy\alembic-1.8.6
set ALEMBIC_INCLUDE_LOCATION=%ALEMBIC_LOCATION%\include
set ALEMBIC_LIB_LOCATION=%ALEMBIC_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%
set MATERIALX_LOCATION=%UE_THIRD_PARTY_LOCATION%\MaterialX\Deploy\MaterialX-1.38.5
set MATERIALX_LIB_LOCATION=%MATERIALX_LOCATION%\%COMPILER_VERSION_NAME%\%ARCH_NAME%\lib
set MATERIALX_CMAKE_LOCATION=%MATERIALX_LIB_LOCATION%\cmake\MaterialX

set PYTHON_BINARIES_LOCATION=%UE_ENGINE_LOCATION%\Binaries\ThirdParty\Python3\Win64
set PYTHON_EXECUTABLE_LOCATION=%PYTHON_BINARIES_LOCATION%\python.exe
set PYTHON_SOURCE_LOCATION=%UE_THIRD_PARTY_LOCATION%\Python3\Win64
set PYTHON_INCLUDE_LOCATION=%PYTHON_SOURCE_LOCATION%\include
set PYTHON_LIBRARY_LOCATION=%PYTHON_SOURCE_LOCATION%\libs\python311.lib

set UE_MODULE_USD_LOCATION=%~dp0

set BUILD_LOCATION=%UE_MODULE_USD_LOCATION%\Intermediate

rem OpenUSD build products are written into a deployment directory and must
rem then be manually copied from there into place.
set INSTALL_LOCATION=%BUILD_LOCATION%\Deploy\OpenUSD-%OPENUSD_VERSION%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\include

if exist %BUILD_LOCATION% (
    rmdir %BUILD_LOCATION% /S /Q)

mkdir %BUILD_LOCATION%
pushd %BUILD_LOCATION%

echo Configuring build for OpenUSD version %OPENUSD_VERSION%...
cmake -G "Visual Studio 17 2022" %OPENUSD_SOURCE_LOCATION%^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_LOCATION%"^
    -DCMAKE_PREFIX_PATH="%IMATH_CMAKE_LOCATION%;%MATERIALX_CMAKE_LOCATION%"^
    -DTBB_INCLUDE_DIR="%TBB_INCLUDE_LOCATION%"^
    -DTBB_LIBRARY="%TBB_LIB_LOCATION%"^
    -DBoost_NO_BOOST_CMAKE=ON^
    -DBoost_NO_SYSTEM_PATHS=ON^
    -DBOOST_INCLUDEDIR="%BOOST_INCLUDE_LOCATION%"^
    -DBOOST_LIBRARYDIR="%BOOST_LIB_LOCATION%"^
    -DPython3_EXECUTABLE="%PYTHON_EXECUTABLE_LOCATION%"^
    -DPython3_INCLUDE_DIR="%PYTHON_INCLUDE_LOCATION%"^
    -DPython3_LIBRARY="%PYTHON_LIBRARY_LOCATION%"^
    -DPXR_BUILD_ALEMBIC_PLUGIN=ON^
    -DPXR_ENABLE_HDF5_SUPPORT=OFF^
    -DOPENSUBDIV_INCLUDE_DIR="%OPENSUBDIV_INCLUDE_DIR%"^
    -DOPENSUBDIV_ROOT_DIR="%OPENSUBDIV_LIB_LOCATION%"^
    -DALEMBIC_INCLUDE_DIR="%ALEMBIC_INCLUDE_LOCATION%"^
    -DALEMBIC_DIR="%ALEMBIC_LIB_LOCATION%"^
    -DPXR_ENABLE_MATERIALX_SUPPORT=ON^
    -DBUILD_SHARED_LIBS=ON^
    -DPXR_BUILD_TESTS=OFF^
    -DPXR_BUILD_EXAMPLES=OFF^
    -DPXR_BUILD_TUTORIALS=OFF^
    -DPXR_BUILD_USD_TOOLS=OFF^
    -DPXR_BUILD_IMAGING=ON^
    -DPXR_BUILD_USD_IMAGING=ON^
    -DPXR_BUILD_USDVIEW=OFF^
    -DCMAKE_CXX_FLAGS="/Zm150 /DBOOST_ALL_NO_LIB"
if %errorlevel% neq 0 exit /B %errorlevel%

echo Building OpenUSD for Release...
cmake --build . --config Release -j8
if %errorlevel% neq 0 exit /B %errorlevel%

echo Installing OpenUSD for Release...
cmake --install . --config Release
if %errorlevel% neq 0 exit /B %errorlevel%

popd

set INSTALL_BIN_LOCATION=%INSTALL_LOCATION%\bin
set INSTALL_LIB_LOCATION=%INSTALL_LOCATION%\lib

echo Removing command-line tools...
rmdir /S /Q "%INSTALL_BIN_LOCATION%"

echo Moving shared libraries to bin directory...
mkdir %INSTALL_BIN_LOCATION%
move "%INSTALL_LIB_LOCATION%\*.dll" "%INSTALL_BIN_LOCATION%"
if exist "%INSTALL_LIB_LOCATION%\*.pdb" (
    move "%INSTALL_LIB_LOCATION%\*.pdb" "%INSTALL_BIN_LOCATION%"
)

echo Moving built-in OpenUSD plugins to UsdResources plugins directory...
set INSTALL_RESOURCES_LOCATION=%INSTALL_LOCATION%\Resources\UsdResources\Win64
set INSTALL_RESOURCES_PLUGINS_LOCATION=%INSTALL_RESOURCES_LOCATION%\plugins
mkdir %INSTALL_RESOURCES_LOCATION%
move "%INSTALL_LIB_LOCATION%\usd" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"

echo Moving OpenUSD plugin shared libraries to bin directory...
set INSTALL_PLUGIN_LOCATION=%INSTALL_LOCATION%\plugin
set INSTALL_PLUGIN_USD_LOCATION=%INSTALL_PLUGIN_LOCATION%\usd
move "%INSTALL_PLUGIN_USD_LOCATION%\*.dll" "%INSTALL_BIN_LOCATION%"
if exist "%INSTALL_PLUGIN_USD_LOCATION%\*.pdb" (
    move "%INSTALL_PLUGIN_USD_LOCATION%\*.pdb" "%INSTALL_BIN_LOCATION%"
)

echo Moving OpenUSD plugin import libraries to lib directory...
move "%INSTALL_PLUGIN_USD_LOCATION%\*.lib" "%INSTALL_LIB_LOCATION%"

echo Removing top-level OpenUSD plugins plugInfo.json file...
del "%INSTALL_PLUGIN_USD_LOCATION%\plugInfo.json"

echo Moving OpenUSD plugin resource directories to UsdResources plugins directory
move "%INSTALL_PLUGIN_USD_LOCATION%\hdStorm" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"
move "%INSTALL_PLUGIN_USD_LOCATION%\sdrGlslfx" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"
move "%INSTALL_PLUGIN_USD_LOCATION%\usdAbc" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"
move "%INSTALL_PLUGIN_USD_LOCATION%\usdShaders" "%INSTALL_RESOURCES_PLUGINS_LOCATION%"

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
