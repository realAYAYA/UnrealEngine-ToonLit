@echo off

echo This batch file, individually converts all discovered ucap files in the specific directory, into Oodle-example-code compatible .bin files
echo.


REM This batch file should be run from \engine\plugins\compression\oodlenetwork
set BaseFolder="..\..\..\.."

if exist %BaseFolder:"=%\Engine goto SetUEEditor

echo Could not locate Engine folder. This .bat must be run from \engine\plugins\compression\oodlenetwork
goto End


:SetUEEditor
set UEEditorLoc="%BaseFolder:"=%\Engine\Binaries\Win64\UnrealEditor.exe"

if exist %UEEditorLoc:"=% goto GetGame

echo Could not locate UnrealEditor.exe
goto End


:GetGame
set /p GameName=Type the name of the game you are working with: 
echo.


:GetCaptureDir
set /p CaptureDir=Paste the directory where the ucap files are located
echo.

if exist %CaptureDir:"=% goto GetOutputDir

echo Could not locate capture files directory
goto End


:GetOutputDir
set /p OutputDir=Paste the directory where the .bin files should be output to
echo.

if exist %OutputDir:"=% goto AutoGenDictionaries

echo Could not locate output directory
goto End



:AutoGenDictionaries
set DebugDumpParms=-run=OodleNetworkTrainerCommandlet DebugDump
set PreDumpCmdLine=%GameName:"=% %DebugDumpParms% %OutputDir:"=% %CaptureDir:"=%
set PostDumpCmdLine=-forcelogflush

echo Executing dictionary generation commandlet - commandline:
echo %UEEditorLoc:"=% %PreDumpCmdLine:"=% %PostDumpCmdLine:"=%


%UEEditorLoc:"=% %PreDumpCmdLine:"=% %PostDumpCmdLine:"=%

echo.


if %errorlevel%==0 goto End

echo WARNING! Detected error, dictionaries may not have been generated. Check output and logfile for errors.
pause


:End
echo Execution complete.
pause


REM Put nothing past here.

