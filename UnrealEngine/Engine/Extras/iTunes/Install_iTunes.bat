@echo off
echo.
if NOT "%1"=="" goto install
echo This batch file is intended to be called by Turnkey and should not be run independently.
goto end

:install
echo This batch file will try to unninstall any installed iTunes and install the version retrieved by Turnkey. 
echo.

echo.
echo Trying to uninstall current iTunes
echo.
pause

wmic product where name="iTunes" call uninstall/nointeractive

echo.
echo Trying to install downloaded iTunes
echo.
pause

%1