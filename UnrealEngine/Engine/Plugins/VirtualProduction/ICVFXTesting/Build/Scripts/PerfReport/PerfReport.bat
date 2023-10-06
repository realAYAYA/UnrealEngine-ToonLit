@echo off
call %~dp0\SetupEnv.bat

if "%~1"=="" (
  %CSVToolsPath%\PerfReportTool
  goto exit
)

if "%~2"=="" (
  @echo on
  %CSVToolsPath%\PerfReportTool -reportxmlbasedir %~dp0 -csv %1
  goto exit
) 

%CSVToolsPath%\PerfReportTool -reportxmlbasedir %~dp0 %* 

:exit
