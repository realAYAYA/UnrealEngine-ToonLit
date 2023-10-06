@echo off
call %~dp0\SetupEnv.bat

if [%~1]==[] (
  %CSVToolsPath%\PerfReportTool -reporttype=FXReport
  goto exit
)

if [%~2]==[] (
  @echo on
  %CSVToolsPath%\PerfReportTool -reporttype FXReport -reportxmlbasedir %~dp0 -csv %1
  goto exit
) 

%CSVToolsPath%\PerfReportTool -reporttype FXReport -reportxmlbasedir %~dp0 %* 

:exit
