@echo off

REM * this unzips the archive 7zip and renames extraction dir to a generic name used by the build scripts (google-test-source)
REM * uncompress_and_patch.bat 

"c:\Program Files\7-Zip\7z.exe" x %1

rem rename the archive extraction directory to what the build batch file wants
rename %~n1 google-test-source



