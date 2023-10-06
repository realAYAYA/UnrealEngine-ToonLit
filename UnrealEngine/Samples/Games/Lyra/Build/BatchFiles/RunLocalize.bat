pushd "%~dp0..\..\..\..\.."
REM Pick one of the localizers that you've configured.

REM =======================
REM CrowdIn
REM =======================
REM call .\Engine\Build\BatchFiles\RunUAT.bat Localize -p4 -UEProjectDirectory="Samples\Games\Lyra" -UEProjectName=Lyra -LocalizationBranch="Main" -LocalizationProjectNames=Game -LocalizationProvider=XLoc_Sample

REM =======================
REM LocX
REM =======================
REM call .\Engine\Build\BatchFiles\RunUAT.bat Localize -p4 -UEProjectDirectory="Samples\Games\Lyra" -UEProjectName=Lyra -LocalizationBranch="Main" -LocalizationProjectNames=Game -LocalizationProvider=Crowdin_Sample
pause
