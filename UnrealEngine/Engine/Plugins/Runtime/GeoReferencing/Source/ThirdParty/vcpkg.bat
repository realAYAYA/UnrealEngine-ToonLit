@echo off

setlocal

:: this is a tag in the vcpkg repository
set VCPKG_VERSION=2021.05.12

:: this is where the artifacts get installed
set VCPKG_INSTALLED=vcpkg-installed

:: setup by Engine\Android\SetupAndroid.bat
set ANDROID_NDK_HOME=%NDKROOT%

:: cleanup the git repo
if exist "%~dp0vcpkg\" echo:
if exist "%~dp0vcpkg\" echo === Tidying up vcpkg ===
if exist "%~dp0vcpkg\" rmdir /s /q "%~dp0vcpkg"

:: cleanup the prior artifacts
if exist "%~dp0%VCPKG_INSTALLED%\" echo:
if exist "%~dp0%VCPKG_INSTALLED%\" echo === Tidying up %VCPKG_INSTALLED% ===
if exist "%~dp0%VCPKG_INSTALLED%\" rmdir /s /q "%~dp0%VCPKG_INSTALLED%"

echo:
echo === Cloning vcpkg to %~dp0vcpkg ===
git clone https://github.com/microsoft/vcpkg.git --depth 1 --branch %VCPKG_VERSION% "%~dp0vcpkg"

echo:
echo === Bootstrapping vcpkg ===
:: -disableMetrics in important to avoid Malwarebytes quarantine the vcpkg file. 
call "%~dp0vcpkg\bootstrap-vcpkg.bat" -disableMetrics

:: build for each triplet
:: --editable leaves the source in the buildtree for easy local debugging and patch generation
for %%x in (overlay-x64-windows overlay-x64-uwp overlay-arm64-uwp x64-android arm64-android) do (
    echo:
    echo === Running vcpkg ===
    "%~dp0vcpkg\vcpkg.exe" install --editable --x-install-root="%~dp0%VCPKG_INSTALLED%" --overlay-ports=./overlay-ports --overlay-triplets=./overlay-triplets --triplet=%%x "proj4[core,database]"
    if ERRORLEVEL 1 exit /b 1

    echo:
    echo === Reconciling %VCPKG_INSTALLED% artifacts ===
    for /f "delims=" %%f in ("%~dp0%VCPKG_INSTALLED%\%%x") do p4 reconcile "%%~ff\..."
)

echo:
echo === Refreshing PROJ data files ===

:: destroy the target
attrib -r "%~dp0..\..\Resources\PROJ\*.*" /s
rmdir /s /q "%~dp0..\..\Resources\PROJ"

:: copy the files
robocopy /MIR /MT "%~dp0%VCPKG_INSTALLED%\overlay-x64-windows\share\proj4" "%~dp0..\..\Resources\PROJ"

:: delete some extra stuff
del "%~dp0..\..\Resources\PROJ\*.cmake"
del "%~dp0..\..\Resources\PROJ\vcpkg*.*"

:: reconcile in p4 (for /f will handle relative paths that p4 can't handle)
for /f "delims=" %%f in ("%~dp0..\..\Resources\PROJ") do p4 reconcile "%%~ff\..."

endlocal
