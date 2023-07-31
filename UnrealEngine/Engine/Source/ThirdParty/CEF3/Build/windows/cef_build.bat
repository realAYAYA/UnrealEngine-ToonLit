@echo off
SETLOCAL ENABLEEXTENSIONS
IF ERRORLEVEL 1 ECHO Unable to enable extensions

IF NOT defined CEF_BRANCH ( set CEF_BRANCH=4577 )
IF NOT DEFINED GN_DEFINES (
    echo "Missing GN_DEFINES variable, make sure it was set via your Docker build args."
    exit /b 1
)
set GN_ARGUMENTS=--ide=vs2019 --sln=cef --filters=//cef/*

REM work around a docker for windows/python bug and force utf-8 encoding
set PYTHONIOENCODING=utf-8

call "c:\program files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.19041.0
set vs2019_install=c:\program files (x86)\Microsoft Visual Studio\2019\BuildTools

set CEF_USE_GN=1
echo.
echo.
echo "## Updating CEF depot tools"
echo.
echo.
cd \code\
if exist "c:\temp\code\cef_build.bat" (
    copy c:\temp\code\* . /y
    mkdir patches
    copy c:\temp\code\patches\* patches\
    REM delete the one in temp so we don't bootstrap into it again
    del c:\temp\code\cef_build.bat
    REM don't call here, just run this one again
    c:\code\cef_build.bat %*
)

if not exist "c:\code\depot_tools" (
    mkdir \code\depot_tools
    powershell -Command Expand-Archive C:/temp/depot_tools.zip -DestinationPath "C:/code/depot_tools"
    powershell -Command $env:Path = 'c:\code\depot_tools\;' + $env:Path;setx /M PATH $env:Path
)
cd \code\depot_tools
call update_depot_tools

IF "%~1"=="" goto :CREATEPROJECT
IF "%~1"=="build" goto :BUILD
IF "%~1"=="package" goto :PACKAGE

:CREATEPROJECT

if exist "c:\code\chromium\src\out\" (
    echo Projects already created in chromium\src\out, manually delete this folder to before recreating packages
    choice.exe /C YN /N /M "Continue build anyway [Y/N]?"
    if ERRORLEVEL 2 (
        echo Exiting
        goto :EXIT
    )
)

echo.
echo.
echo "## Building CEF branch %CEF_BRANCH%"
echo.
echo.
cd \code
if not exist "c:\code\automate\automate-git.py" (
    mkdir automate
    copy c:\temp\automate-git.py c:\code\automate\automate-git.py
)
call python c:\code\automate\automate-git.py --branch=%CEF_BRANCH% --download-dir=c:\code --no-distrib --no-build --force-update --no-depot-tools-update
if not %errorlevel% == 0 (
    echo "Failed getting CEF build"
    goto :EXIT
)

echo.
echo.
echo "### Applying local patches to build"
echo.
echo.
cd c:\code\chromium\src\cef\
For /R c:\code\patches\ %%G IN (*.*) do (
    echo "###   Applying %%G"
    call git apply %%G
    if not %errorlevel% == 0 (
        echo Failed applying CEF patches
        echo Exiting...
        REM exit /b 1
        goto :EXIT
    )
)
echo.
echo.
echo "### Running cef_create_projects"
echo.
echo.
call cef_create_projects.bat

:BUILD
if not exist "c:\code\chromium\src\out\" (
    echo Project build folder missing, have you run the create projects step? 
    echo Exiting...
    goto :EXIT
)


cd c:\code\chromium\src\
echo.
echo.
echo "### Starting x64 debug build"
echo.
echo.
call ninja -C out\Debug_GN_x64 cefclient
if not %errorlevel% == 0 (
    echo "Failed getting CEF build"
    goto :EXIT
)
REM call ninja -C out\Debug_GN_x64_sandbox cef_sandbox 
echo.
echo.
echo "### Starting x64 release build"
echo.
echo.
call ninja -C out\Release_GN_x64 cefclient 
if not %errorlevel% == 0 (
    echo "Failed getting CEF build"
    goto :EXIT
)
REM call ninja -C out\Release_GN_x64_sandbox cef_sandbox 

echo.
echo.
echo "### Starting x86 debug build"
echo.
echo.
call ninja -C out\Debug_GN_x86 cefclient 
if not %errorlevel% == 0 (
    echo "Failed getting CEF build"
    goto :EXIT
)
REM call ninja -C out\Debug_GN_x86_sandbox cef_sandbox 
echo.
echo.
echo "### Starting x86 release build"
echo.
echo.
call ninja -C out\Release_GN_x86 cefclient 
if not %errorlevel% == 0 (
    echo "Failed getting CEF build"
    goto :EXIT
)
REM call ninja -C out\Release_GN_x86_sandbox cef_sandbox 

IF "%~1"=="nowrapper_build" goto :SUCCESSEXIT
IF "%~1"=="build" goto :SUCCESSEXIT
:PACKAGE
echo.
echo.
echo "### Packaging release"
echo.
echo.
cd c:\code\chromium\src\cef\tools
call make_distrib --ninja-build --no-archive
call make_distrib --ninja-build --no-archive --x64-build

REM copy the resulting build to a folder inside the container to allow the cmake steps below to work
REM this is working around https://github.com/docker/for-win/issues/829 
echo "### Copying distrib folder into docker image for wrapper build"
mkdir c:\build\cef_output\binary_distrib
xcopy c:\code\chromium\src\cef\binary_distrib c:\build\cef_output\binary_distrib /s /e /h /q /y
cd c:\build\cef_output

echo "### Building CEF wrapper library"
call "c:\program files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.19041.0
set vs2019_install=c:\program files (x86)\Microsoft Visual Studio\2019\BuildTools
REM clear the RC variable that we inherited from the build
set RC=
FOR /F "tokens=*" %%g IN ('dir /b c:\build\cef_output\binary_distrib\*windows64') do (SET BINARY_FOLDER=%%g)
set BINARY_DIST_DIR=c:\build\cef_output\binary_distrib\%BINARY_FOLDER%
rmdir /s /q wrapper_build_x64
mkdir wrapper_build_x64
pushd wrapper_build_x64
cmake -G "Visual Studio 14" -A x64 -DUSE_SANDBOX=OFF -DCMAKE_SYSTEM_VERSION=8.1 -DCEF_RUNTIME_LIBRARY_FLAG="/MD" -DCEF_DEBUG_INFO_FLAG="/Zi" -DUSE_ATL=OFF %BINARY_DIST_DIR%
if not %errorlevel% == 0 (
    echo Failed creating cmake build for wrapper
    goto :EXIT
)
msbuild cef.sln /p:Configuration="Release" /p:Platform="x64" /target:libcef_dll_wrapper
if not %errorlevel% == 0 (
    echo Failed building wrapper
    goto :EXIT
)
msbuild cef.sln /p:Configuration="Debug" /p:Platform="x64" /target:libcef_dll_wrapper
if not %errorlevel% == 0 (
    echo Failed building wrapper
    goto :EXIT
)

mkdir "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Release"
mkdir "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Debug"
copy libcef_dll_wrapper\Release\libcef_dll_wrapper.lib "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Release\"
copy libcef_dll_wrapper\Debug\libcef_dll_wrapper.lib "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Debug\"
copy libcef_dll_wrapper\Debug\libcef_dll_wrapper.pdb "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Debug\"
popd

FOR /F "tokens=*" %%g IN ('dir /b c:\build\cef_output\binary_distrib\*windows32') do (SET BINARY_FOLDER=%%g)
set BINARY_DIST_DIR=c:\build\cef_output\binary_distrib\%BINARY_FOLDER%
rmdir /s /q wrapper_build_x86
mkdir wrapper_build_x86
pushd wrapper_build_x86
cmake -G "Visual Studio 14" -A win32 -DUSE_SANDBOX=OFF -DCMAKE_SYSTEM_VERSION=8.1 -DCEF_RUNTIME_LIBRARY_FLAG="/MD" -DCEF_DEBUG_INFO_FLAG="/Zi" -DUSE_ATL=OFF %BINARY_DIST_DIR%
if not %errorlevel% == 0 (
    echo Failed creating cmake build for wrapper
    goto :EXIT
)
msbuild cef.sln /p:Configuration="Release" /p:Platform="win32" /target:libcef_dll_wrapper
if not %errorlevel% == 0 (
    echo Failed building wrapper
    goto :EXIT
)
msbuild cef.sln /p:Configuration="Debug" /p:Platform="win32" /target:libcef_dll_wrapper
if not %errorlevel% == 0 (
    echo Failed building wrapper
    goto :EXIT
)

mkdir "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Release"
mkdir "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Debug"
copy libcef_dll_wrapper\Release\libcef_dll_wrapper.lib "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Release\"
copy libcef_dll_wrapper\Debug\libcef_dll_wrapper.lib "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Debug\"
copy libcef_dll_wrapper\Debug\libcef_dll_wrapper.pdb "%BINARY_DIST_DIR%\VS2015\libcef_dll_wrapper\Debug\"
popd

REM zip the output up to our root folder to copy out
c:\7zip\7z a c:\packages_cef.zip c:\build\cef_output\binary_distrib

echo "###"
echo "### Build complete. Quit this shell to allow the build.bat file to copy the result locally."
echo "###"
echo "### You can also run "docker cp cef3_build:/packages_cef.zip ." to extract the build locally"
echo "###"
echo "###"

:SUCCESSEXIT
exit /b 0
:EXIT
exit /b 1