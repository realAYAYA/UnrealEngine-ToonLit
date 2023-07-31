@echo off

set CWD=%cd%
set CHROME_USER_DATA=%CWD%/.tmp_chrome_data/

echo "Opening chrome..."


set CHROME="C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"
if exist "C:\Program Files\Google\Chrome\Application\chrome.exe" (
	set CHROME="C:\Program Files\Google\Chrome\Application\chrome.exe"
)

REM --allow-file-access-from-files allow to load a file from a file:// webpage required for GPUDumpViewer.html to work.
REM --user-data-dir is required to force chrome to open a new instance so that --allow-file-access-from-files is honored.
%CHROME% "file://%CWD%/GPUDumpViewer.html" --allow-file-access-from-files --new-window --incognito --user-data-dir="%CHROME_USER_DATA%"

echo "Closing chrome..."

REM Wait for 2s to shut down so that CHROME_USER_DATA can be deleted completly
timeout /t 2 /nobreak > NUL

rmdir /S /Q "%CHROME_USER_DATA%"

