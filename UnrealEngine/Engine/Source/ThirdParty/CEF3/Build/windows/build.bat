@echo off
SETLOCAL ENABLEEXTENSIONS
IF ERRORLEVEL 1 ECHO Unable to enable extensions

REM Build cef as an official build
set GN_DEFINES="is_component_build=false enable_precompiled_headers=false is_official_build=true enable_remoting=false use_thin_lto=false"
set CEF_BRANCH=4430

IF "%~1"=="" goto :FULLSYNC
IF "%~1"=="build" goto :BUILDONLY
IF "%~1"=="package" goto :WRAPPERBUILD

:FULLSYNC
docker build -f Dockerfile_base -t cef3 .
if not %errorlevel% == 0 (
    echo Failed sync and build of CEF. Check above for errors
    goto :EXIT
)
docker build -f Dockerfile_build -t cef3_build --build-arg CEF_BRANCH=%CEF_BRANCH% --build-arg GN_DEFINES=%GN_DEFINES% .
if not %errorlevel% == 0 (
    echo Failed sync and build of CEF. Check above for errors
    goto :EXIT
)

echo Performing full sync and build
REM Delete any old CEF3 build images that may exist
docker rm cef3_build
REM Run docker in HyperV mode to sync the initial code, needed because Git in process isolation mode fails badly due to the filesystem hooks
docker run --name cef3_build --storage-opt size=120G --memory=30g --cpus=%NUMBER_OF_PROCESSORS% -v cef3_code_volume:C:/code --isolation=hyperv -it cef3_build c:\temp\code\cef_build.bat nowrapper_build
if not %errorlevel% == 0 (
    echo Failed sync and build of CEF. Check above for errors
    goto :EXIT
)

goto :WRAPPERBUILD
:BUILDONLY
echo Building existing sync
REM Delete any old CEF3 build images that may exist
docker rm cef3_build
REM Run docker in HyperV mode to sync the initial code, needed because Git in process isolation mode fails badly due to the filesystem hooks
docker run --name cef3_build --storage-opt size=120G --memory=30g --cpus=%NUMBER_OF_PROCESSORS% -v cef3_code_volume:C:/code --isolation=hyperv -it cef3_build c:\temp\code\cef_build.bat build
if not %errorlevel% == 0 (
    echo Failed sync and build of CEF. Check above for errors
    goto :EXIT
)

:WRAPPERBUILD
docker rm cef3_build
REM Run docker in HyperV mode mode for the wrapper build, slower but process isolation mostly doesn't work on desktop windows
docker run --name cef3_build --storage-opt size=120G --memory=30g --cpus=%NUMBER_OF_PROCESSORS% -v cef3_code_volume:C:/code --isolation=hyperv -it cef3_build c:\temp\code\cef_build.bat package
if not %errorlevel% == 0 (
    echo Failed packaging CEF. Check above for errors
    goto :EXIT
)
docker cp cef3_build:/packages_cef.zip .
echo "###"
echo "### The packages_cef.zip file now contains the Windows CEF build."

:EXIT
