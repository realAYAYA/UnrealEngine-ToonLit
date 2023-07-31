@echo off

echo This batch file, enables the Oodle plugin, and enables it as a packet handler.
echo.


REM This batch file should be run from \engine\plugins\compression\oodlenetwork
set BaseFolder="..\..\..\.."

if exist %BaseFolder:"=%\Engine goto SetUEEditor

echo Could not locate Engine folder. This .bat must be run from \engine\plugins\compression\oodlenetwork
goto End


:SetUEEditor
set UEEditorLoc="%BaseFolder:"=%\Engine\Binaries\Win64\UnrealEditor.exe"

if exist %UEEditorLoc:"=% goto GetGame

echo Could not locate UEEditor.exe
goto End


:GetGame
set /p GameName=Type the name of the game you are working with: 
echo.



:EnablePlugin
set EnableCommandletParms=-run=plugin enable OodleNetwork
set FinalEnableCmdLine=%GameName:"=% %EnableCommandletParms% -forcelogflush

echo Executing plugin enable commandlet - commandline:
echo %FinalEnableCmdLine%

@echo on
%UEEditorLoc:"=% %FinalEnableCmdLine%
@echo off
echo.


if %errorlevel%==0 goto EnableHandler

echo WARNING! Detected error, plugin may not have been enabled. Will attempt to run Oodle enable commandlet.
pause


:EnableHandler
set HandlerCommandletParms=-run=OodleNetworkTrainerCommandlet enable
set FinalHandlerCmdLine=%GameName:"=% %HandlerCommandletParms% -forcelogflush


echo Executing Oodle PacketHandler enable commandlet - commandline:
echo %FinalHandlerCmdLine%

@echo on
%UEEditorLoc:"=% %FinalHandlerCmdLine%
@echo off
echo.


if %errorlevel%==0 goto End

echo WARNING! Detected error when executing PacketHandler enable commandlet. Review the logfile.


:End
echo Execution complete.
pause


REM Put nothing past here.

