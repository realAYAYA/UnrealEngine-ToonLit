@setlocal
@echo off
:: Change to directory where batch file is located.  We'll restore this later with "popd"
pushd %~dp0


:: Main Script
SET FOUND_VERSIONS=
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist %VSWHERE% (
    for /F "tokens=1,2" %%i in ('%VSWHERE% -all') do (
        if "%%i"=="catalog_productLineVersion:" (
          call :InstallVersion %%j
        )
    )
)

if "%FOUND_VERSIONS%" equ "" (
  echo ERROR: Could not find a valid version of Visual Studio installed
)
goto :End



:: function InstallVersion // InstallVersion <VersionName>
:InstallVersion

set VERSION_NAME=%1
:: Strip leading whitespace from version name
for /f "tokens=* delims= " %%a in ("%VERSION_NAME%") do set VERSION_NAME=%%a
if "%VERSION_NAME%"=="" exit /b 0

set ALREADY_EXISTS=0
for /F %%i in ('echo. %FOUND_VERSIONS% ^| find "_SEP_%VERSION_NAME%_SEP_"') do set ALREADY_EXISTS=1
if %ALREADY_EXISTS%==1 exit /b 0
set FOUND_VERSIONS=%FOUND_VERSIONS%_SEP_%VERSION_NAME%_SEP_

set VISUALIZER_DIR="%USERPROFILE%\Documents\Visual Studio %VERSION_NAME%\Visualizers"
if not exist %VISUALIZER_DIR% mkdir %VISUALIZER_DIR%

echo Installing visualizers for Visual Studio %VERSION_NAME%...
echo     copy Unreal.natstepfilter %VISUALIZER_DIR%
copy     Unreal.natstepfilter %VISUALIZER_DIR% 1>nul

exit /b 0


:: goto-label :End // restores settings and exits
:End
popd
pause
endlocal
exit /b
