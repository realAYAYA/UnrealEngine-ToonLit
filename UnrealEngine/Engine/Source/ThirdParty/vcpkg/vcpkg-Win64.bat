@echo off

setlocal

:: this is a tag in the vcpkg repository
set VCPKG_VERSION=2022.03.10

:: enable manifest mode
set VCPKG_FEATURE_FLAGS=manifests

:: the triplet to build
set VCPKG_TRIPLETS=x64-windows-static-md-v142;x64-windows-static-v142

:: the Unreal platform
set UE_PLATFORM=Win64

set VCPKG_DIR=%TEMP%\vcpkg-%UE_PLATFORM%-%VCPKG_VERSION%

pushd %~dp0

echo:
echo === Checking out vcpkg to %VCPKG_DIR% ===
git clone --single-branch --branch %VCPKG_VERSION% -- https://github.com/microsoft/vcpkg.git %VCPKG_DIR%

echo:
echo === Bootstrapping vcpkg ===
call %VCPKG_DIR%\bootstrap-vcpkg.bat -disableMetrics

echo:
echo === Making %UE_PLATFORM% artifacts writeable ===
attrib -R .\%UE_PLATFORM%\*.* /s

echo:
echo === Running vcpkg in manifest mode ===
FOR %%T IN (%VCPKG_TRIPLETS%) DO (
    mkdir .\%UE_PLATFORM%\%%T
    copy /Y .\vcpkg.json .\%UE_PLATFORM%\%%T\vcpkg.json

    %VCPKG_DIR%\vcpkg.exe install ^
        --overlay-ports=.\overlay-ports ^
        --overlay-triplets=.\overlay-triplets ^
        --x-manifest-root=.\%UE_PLATFORM%\%%T ^
        --x-packages-root=.\%UE_PLATFORM%\%%T ^
        --triplet=%%T
)

echo:
echo === Reconciling %UE_PLATFORM% artifacts ===
p4 reconcile .\%UE_PLATFORM%\...

popd

endlocal