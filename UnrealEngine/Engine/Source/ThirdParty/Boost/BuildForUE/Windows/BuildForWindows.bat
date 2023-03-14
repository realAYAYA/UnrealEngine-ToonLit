@echo off

:: Copyright Epic Games, Inc. All Rights Reserved.

:: To build the Boost libraries, you need to have "Build Tools for Visual Studio" installed: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019

setlocal EnableDelayedExpansion

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
set PATH_SCRIPT=%~dp0
set BOOST_VERSION_FILENAME=boost_%BOOST_VERSION:.=_%
set BOOST_ZIP_FILE=%BOOST_VERSION_FILENAME%.zip
set BOOST_INSTALL_PATH=%PATH_SCRIPT%..\..\boost-%BOOST_VERSION:.=_%
set BOOST_INTERMEDIATE_PATH=..\..\..\..\..\Intermediate\ThirdParty\Boost\%BOOST_VERSION_FILENAME%

if %ALREADY_HAVE_SOURCES%==0 (
	:: Remove previous intermediate files to allow for a clean build.
	if exist %BOOST_INTERMEDIATE_PATH% (
		:: Filenames in the intermediate directory are likely too long for tools like 'rmdir' to handle. Instead, we use robocopy to mirror an empty temporary folder, and then delete it.
		echo [%time%] Deleting previous intermediate files in '%BOOST_INTERMEDIATE_PATH%'...
		mkdir "%BOOST_INTERMEDIATE_PATH%_DELETE"
		robocopy "%BOOST_INTERMEDIATE_PATH%_DELETE" "%BOOST_INTERMEDIATE_PATH%" /purge /W:0 /R:0 > NUL
		rmdir "%BOOST_INTERMEDIATE_PATH%_DELETE"
		rmdir "%BOOST_INTERMEDIATE_PATH%"
	)

	:: Create intermediate directory.
	mkdir %BOOST_INTERMEDIATE_PATH%
)

:: Use intermediate directory.
cd %BOOST_INTERMEDIATE_PATH%

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
	echo Expecting sources to already be available at '%BOOST_INTERMEDIATE_PATH%\%BOOST_VERSION_FILENAME%'.
)

:: Build and install or just copy header files.
cd %BOOST_VERSION_FILENAME%
if !BOOST_BUILD_LIBRARIES!==1 (
	:: Bootstrap before build.
	set LOG_FILE=%BOOST_INTERMEDIATE_PATH%\%BOOST_VERSION_FILENAME%_bootstrap.log
	echo [!time!] Bootstrapping Boost %BOOST_VERSION%, see '!LOG_FILE!' for details...
	call bootstrap > !LOG_FILE!
	if not errorlevel 0 goto error
		
	:: Set tool set to current UE tool set.
	set BOOST_TOOLSET=msvc-14.2

	:: Provide user config to provide tool set version and Python configuration.
	set BOOST_PYTHON_PATH=%PATH_SCRIPT%\..\..\..\Python3\Win64
	set BOOST_USER_CONFIG=%PATH_SCRIPT%\user-config.jam

	:: Build all libraries.
	set LOG_FILE=%BOOST_INTERMEDIATE_PATH%\%BOOST_VERSION_FILENAME%_build.log
	echo [!time!] Building Boost %BOOST_VERSION%, see '!LOG_FILE!' for details...
	.\b2 !BOOST_WITH_LIBRARIES! toolset=!BOOST_TOOLSET! --user-config=!BOOST_USER_CONFIG!^
	    --prefix=%BOOST_INSTALL_PATH% --libdir=%BOOST_INSTALL_PATH%\lib\Win64^
		-j8 --hash --build-type=complete --debug-configuration install^
		address-model=64 threading=multi variant=release^
		> !LOG_FILE!
	if not errorlevel 0 goto error

	:: Move header files out of versioned directory.
	if exist %BOOST_INSTALL_PATH%\include (
		pushd %BOOST_INSTALL_PATH%\include\boost-*
	 	move boost ..\ > NUL
	 	cd ..
	 	for /d %%x in (boost-*) do rd /s /q "%%x"
		popd
	)
) else (
	:: Copy header files using robocopy to prevent issues with long file paths.
	if not exist %BOOST_INSTALL_PATH% (
		mkdir %BOOST_INSTALL_PATH%
	)
	set LOG_FILE=%BOOST_INTERMEDIATE_PATH%\%BOOST_VERSION_FILENAME%_robocopy.log
	echo [!time!] Copying header files, see '!LOG_FILE!' for details...
	set HEADERS_SOURCE=boost
	set HEADERS_DESTINATION=%BOOST_INSTALL_PATH%\include\boost
	robocopy !HEADERS_SOURCE! !HEADERS_DESTINATION! /e > !LOG_FILE!
	set ROBOCOPY_SUCCESS=false
	if errorlevel 0 set ROBOCOPY_SUCCESS=true
	if errorlevel 1 set ROBOCOPY_SUCCESS=true
	if !ROBOCOPY_SUCCESS!=="false" goto error
)

:: Print success confirmation and exit.
echo [!time!] Boost %BOOST_VERSION% installed to '%BOOST_INSTALL_PATH%'.
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
echo     BuildForWindows.bat 1.55.0
echo       -- Installs Boost version 1.55.0 as header-only.
echo.
echo     BuildForWindows.bat 1.66.0 iostreams,system,thread
echo       -- Builds and installs Boost version 1.66.0 with iostreams, system, and thread libraries.
echo.
echo     BuildForWindows.bat 1.72.0 all
echo       -- Builds and installs Boost version 1.72.0 with all of its libraries.
exit /B 1
