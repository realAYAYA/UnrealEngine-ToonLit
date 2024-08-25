@echo off

:: Copyright Epic Games, Inc. All Rights Reserved.

:: To build the Boost libraries, you need to have "Build Tools for Visual Studio" installed: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019

setlocal EnableDelayedExpansion

set BUILD_SCRIPT_LOCATION=%~dp0
set UE_MODULE_LOCATION=%BUILD_SCRIPT_LOCATION%..\..

:: Set the following variable to 1 if you already downloaded and extracted the boost sources, and you need to play around with the build configuration.
set ALREADY_HAVE_SOURCES=0

:: Get version from arguments.
set ARG_VERSION=%1

:: Get libraries list from arguments. It is built from all remaining arguments with '=' and ',' being argument delimiters.
set ARG_LIBRARIES=%2
set BOOST_WITH_LIBRARIES=--with-%2
set BOOST_WITH_PYTHON=0
:loop_over_libraries
if "%2"=="python" set BOOST_WITH_PYTHON=1
shift
if [%2]==[] goto after_loop_over_libraries
set ARG_LIBRARIES=%ARG_LIBRARIES%, %2
set BOOST_WITH_LIBRARIES=%BOOST_WITH_LIBRARIES% --with-%2
goto loop_over_libraries
:after_loop_over_libraries

:: Extract version number.
if [%ARG_VERSION%]==[] goto usage
set BOOST_VERSION=%ARG_VERSION%

:: Extract libraries to be built.
set BOOST_BUILD_LIBRARIES=0
if not [!ARG_LIBRARIES!]==[] set BOOST_BUILD_LIBRARIES=1

:: Print arguments to make spotting errors in the arguments easier.
echo Provided arguments:
echo     Boost version: %BOOST_VERSION%
if !BOOST_BUILD_LIBRARIES!==1 (
	echo     Build libraries: !ARG_LIBRARIES!
) else (
	echo     Build libraries: ^<headers-only^>
)
echo.

:: Set up paths and filenames.
set BOOST_VERSION_FILENAME=boost_%BOOST_VERSION:.=_%
set BOOST_ZIP_FILE=%BOOST_VERSION_FILENAME%.zip

set BUILD_LOCATION=%UE_MODULE_LOCATION%\Intermediate

set INSTALL_INCLUDEDIR=include
set INSTALL_LIB_DIR=lib/Win64

set INSTALL_LOCATION=%UE_MODULE_LOCATION%\boost-%BOOST_VERSION:.=_%
set INSTALL_INCLUDE_LOCATION=%INSTALL_LOCATION%\%INSTALL_INCLUDEDIR%
set INSTALL_LIB_LOCATION=%INSTALL_LOCATION%\%INSTALL_LIB_DIR%

if %ALREADY_HAVE_SOURCES%==0 (
	:: Remove previous intermediate files to allow for a clean build.
	if exist %BUILD_LOCATION% (
		:: Filenames in the intermediate directory are likely too long for tools like 'rmdir' to handle. Instead, we use robocopy to mirror an empty temporary folder, and then delete it.
		echo [%time%] Deleting previous intermediate files in '%BUILD_LOCATION%'...
		mkdir "%BUILD_LOCATION%_DELETE"
		robocopy "%BUILD_LOCATION%_DELETE" "%BUILD_LOCATION%" /purge /W:0 /R:0 > NUL
		rmdir "%BUILD_LOCATION%_DELETE"
		rmdir "%BUILD_LOCATION%"
	)

	:: Create intermediate directory.
	mkdir %BUILD_LOCATION%
)

:: Use intermediate directory.
cd %BUILD_LOCATION%

if %ALREADY_HAVE_SOURCES%==0 (
	:: Download ZIP files.
	set BOOST_URL=https://boostorg.jfrog.io/artifactory/main/release/%BOOST_VERSION%/source/%BOOST_ZIP_FILE%
	echo [!time!] Downloading !BOOST_URL!...
	powershell -Command "(New-Object Net.WebClient).DownloadFile('!BOOST_URL!', '%BOOST_ZIP_FILE%')"
	if not errorlevel 0 goto error

	:: Extract ZIP file.
	echo [!time!] Extracting %BOOST_ZIP_FILE%...
	tar -xf %BOOST_ZIP_FILE%
	if not errorlevel 0 goto error
) else (
	echo Expecting sources to already be available at '%BUILD_LOCATION%\%BOOST_VERSION_FILENAME%'.
)

:: Build and install or just copy header files.
cd %BOOST_VERSION_FILENAME%
if !BOOST_BUILD_LIBRARIES!==1 (
	:: Bootstrap before build.
	set LOG_FILE=%BUILD_LOCATION%\%BOOST_VERSION_FILENAME%_bootstrap.log
	echo [!time!] Bootstrapping Boost %BOOST_VERSION%, see '!LOG_FILE!' for details...
	call .\bootstrap.bat > !LOG_FILE! 2>&1
	if not errorlevel 0 goto error
		
	:: Set tool set to current UE tool set.
	set BOOST_TOOLSET=msvc-14.3

	:: Provide user config to provide tool set version and Python configuration.
	set BOOST_USER_CONFIG=%BUILD_SCRIPT_LOCATION%\user-config.jam

	:: Build all libraries.
	set LOG_FILE=%BUILD_LOCATION%\%BOOST_VERSION_FILENAME%_build.log
	echo [!time!] Building Boost %BOOST_VERSION%, see '!LOG_FILE!' for details...
	.\b2.exe ^
		--prefix=%INSTALL_LOCATION%^
		--includedir=%INSTALL_INCLUDE_LOCATION%^
		--libdir=%INSTALL_LIB_LOCATION%^
		-j8^
		address-model=64^
		threading=multi^
		variant=release^
		%BOOST_WITH_LIBRARIES%^
		--user-config=!BOOST_USER_CONFIG!^
		--hash^
		--build-type=complete^
		--layout=tagged^
		--debug-configuration^
		toolset=!BOOST_TOOLSET!^
		install^
		> !LOG_FILE! 2>&1
	if not errorlevel 0 goto error
) else (
	:: Copy header files using robocopy to prevent issues with long file paths.
	if not exist %INSTALL_LOCATION% (
		mkdir %INSTALL_LOCATION%
	)
	set LOG_FILE=%BUILD_LOCATION%\%BOOST_VERSION_FILENAME%_robocopy.log
	echo [!time!] Copying header files, see '!LOG_FILE!' for details...
	set HEADERS_SOURCE=boost
	set HEADERS_DESTINATION=%INSTALL_LOCATION%\include\boost
	robocopy !HEADERS_SOURCE! !HEADERS_DESTINATION! /e > !LOG_FILE! 2>&1
	set ROBOCOPY_SUCCESS=false
	if errorlevel 0 set ROBOCOPY_SUCCESS=true
	if errorlevel 1 set ROBOCOPY_SUCCESS=true
	if !ROBOCOPY_SUCCESS!=="false" goto error
)

:: Print success confirmation and exit.
echo [!time!] Boost %BOOST_VERSION% installed to '%INSTALL_LOCATION%'.
echo [!time!] Done.
exit /B 0


:: Helper functions

:error
:: Print generic error message and exit.
echo [!time!] Last command returned an error!
echo [!time!] Abort.
exit /B 1

:usage
:: Print usage and exit.
echo Invalid arguments.
echo.
echo Usage:
echo.
echo     BuildForWindows.bat ^<version^> [^<comma-separated-library-name-list^>]
echo.
echo Usage examples:
echo.
echo     BuildForWindows.bat 1.82.0
echo       -- Installs Boost version 1.82.0 as header-only.
echo.
echo     BuildForWindows.bat 1.82.0 iostreams,system,thread
echo       -- Builds and installs Boost version 1.82.0 with iostreams, system, and thread libraries.
echo.
echo     BuildForWindows.bat 1.82.0 all
echo       -- Builds and installs Boost version 1.82.0 with all of its libraries.
exit /B 1

endlocal
