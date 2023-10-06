@echo off
echo.
echo This batch file will set the permissions required to allow Unreal Engine to communicate with iTunes downloaded from the Windows 10 Store. 
echo.

pause

takeown /F "C:\Program Files\WindowsApps" > %temp%\itunes_perm_temp.txt 2>&1 
type %temp%\itunes_perm_temp.txt
for /f "tokens=1" %%i in (%temp%\itunes_perm_temp.txt) do if "%%i"=="ERROR:" goto err

for /d %%f in ("C:\Program Files\WindowsApps\AppleInc.iTunes*") do takeown /F "%%f" /R
for /d %%f in ("C:\Program Files\WindowsApps\AppleInc.iTunes*") do icacls "%%f" /grant "%USERNAME%":F /T

echo.
echo Permissions updated.
echo.
pause

goto done
:err

echo.
echo Permission update failed. Are you running this from an Administrator command prompt?
echo.
pause

:done
del %temp%\itunes_perm_temp.txt