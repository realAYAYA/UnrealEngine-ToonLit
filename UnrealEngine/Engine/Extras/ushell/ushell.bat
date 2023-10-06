@echo off

:: Here be dragons. This file detects how the user has launched ushell.bat by
:: either 1) a shortcut or directly in Explorer, or 2) as part of a script. This
:: is done by examining the cmd.exe command line. If (1) is detected then a new
:: interactive cmd.exe process is started. Otherwise the environment is set up
:: in the current host.

setlocal
set _self=%~f0

:: quotes and separators really upset cmd.exe's interpreter so we remove them
set _cmdline=%cmdcmdline%
set _cmdline="%_cmdline:"=%"
set _cmdline=%_cmdline:&=#%
set _cmdline=%_cmdline:|=#%
call:main %_cmdline:~1,-1%

set _boot_bat="%~dp0\channels\flow\nt\boot.bat"
if "%_breakout%"=="" endlocal & %_boot_bat% %*
endlocal & start /i cmd.exe /d/k "%_boot_bat% %*"

goto:eof

:main
:: we only consider breaking out if the first cmd.exe argument is /c
shift
if not "%~1"=="/c" goto:eof

:: extract the .bat paths script. this reconstructs paths with spaces in
:bat_path_loop
    shift
    if "%1"=="" goto:bat_path_loop_end
    set _bat_path=%_bat_path% %~1
    if not "%~x1"==".bat" goto:bat_path_loop
:bat_path_loop_end

:: we shouldn't think about breaking out if we didn't find ourself
if not "%_bat_path:~1%"=="%_self%" goto:eof

:: don't breakout if the user appears to have used separators
:seperators_loop
    shift
    if "%1"=="" goto:seperators_loop_end
    if "%1"=="#" goto:eof
    if "%1"=="##" goto:eof
    goto:seperators_loop
:seperators_loop_end

set _breakout=1
goto:eof
