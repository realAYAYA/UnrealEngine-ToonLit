@echo off

echo This batch file, goes through the process of building Oodle dictionaries from packet captures.
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
set GameName=%1
if not "%1" == "" goto GetDictionaryOutput
set /p GameName=Type the name of the game you are working with: 
echo.

:GetDictionaryOutput
set DictionaryOutput=%2
if not "%2" == "" goto GetFilter
set /p DictionaryOutput=Type the absolute path and full name of the resulting dictionary file, or Input or Output for the default dictionary location: 
echo.

:GetFilter
set FileFilter=%3
if not "%3" == "" goto GetChangelistFilter
set /p FileFilter=Type a filename filter to filter by, or all for all files: 
echo.

:GetChangelistFilter
set ChangelistFilter=%4
if not "%4" == "" goto GetDirectory
set /p ChangelistFilter=Type a changelist number to filter by, or all for all files: 
echo.

:GetDirectory
set DirectoryRoot=%5
if not "%5" == "" goto AutoGenDictionaries
set /p DirectoryRoot=Type the root directory where the capture files are located: 
echo.

:AutoGenDictionaries
set AutoGenDictionariesParms=-run=OodleNetworkTrainerCommandlet GenerateDictionary %DictionaryOutput% %FileFilter% %ChangelistFilter% all %DirectoryRoot%
set FinalGenCmdLine=%GameName:"=% %AutoGenDictionariesParms% -forcelogflush

echo Executing dictionary generation commandlet - commandline:
echo %FinalGenCmdLine%

@echo on
%UEEditorLoc:"=% %FinalGenCmdLine%
@echo off
echo.


if %errorlevel%==0 goto End

echo WARNING! Detected error, dictionaries may not have been generated. Check output and logfile for errors.
pause


:End
echo Execution complete.

