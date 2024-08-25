:: Copyright Epic Games, Inc. All Rights Reserved.

@echo off
setlocal
chcp 65001 1>nul 2>nul

:: Set up the working directory flow can store its necessities in, and the dir
:: that contains the channels that implement the shell.
set _working="%localappdata%\ushell\.working"
set _channels="%~dp0..\..\..\channels"
if defined flow_working_dir (
    set _working="%flow_working_dir%"
)

:: Call out to user hooks
if exist "%userprofile%\.ushell\hooks\boot.bat" (
    call "%userprofile%\.ushell\hooks\boot.bat"
)

:: Make it easier for users to add their own channels
set _channels=%_channels% "%userprofile%\.ushell\channels"
if defined flow_channels_dir (
    set _channels=%_channels% %flow_channels_dir%
)

:: Provision Python. We shim through a cmd.exe for simpler error handling in
:: provision.bat (see the script's use of call:on_error).
cmd.exe /d/c ""%~dp0\provision.bat" %_working%"
if errorlevel 1 exit /b 1

:: Boot up
set _cookie=%time::=%
set _cookie=%_cookie:~1%
set _cookie=%temp%\ushell\cmd_boot_%_cookie%
%_working%\python\current\flow_python.exe -Xutf8 -Esu "%~dp0..\core\system\boot.py" %_working% %_channels% -- "--bootarg=cmd,%_cookie%" %*

:: If --help was given then we return the user to where they came from. The "127"
:: comes from _flick
if %errorlevel%==127 (
    goto:eof
)

:: Was the Python binary missing? "9009" comes from cmd.exe
if %errorlevel%==9009 (
    echo,
    echo,
    echo ERROR: Missing flow_python.exe from '%_working%\python\current'
    timeout /T 15
    exit 1
)

:: did boot.py fail?
if errorlevel 1 (
    echo,
    echo,
    echo ERROR: boot.py failed [%errorlevel%]
    timeout /T 15
    exit 1
)

endlocal & if exist "%_cookie%" (
    for /f "usebackq delims=" %%d in ("%_cookie%") do (
        %%d
    )
    del /q "%_cookie%"
)
