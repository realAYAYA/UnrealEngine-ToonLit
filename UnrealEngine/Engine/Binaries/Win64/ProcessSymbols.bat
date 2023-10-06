@echo off

rem %1 full path to symstore.exe
rem %2 full path to list of files to process
rem %3 full path to a temporary index file (created by symstore)
rem %4 base path to symbol inputs
rem %5 target product
rem %6 symbol store path

rem create the index file
%1 add /r /f @%2 /x %3 /g %4 /o

rem compress the input files, then delete them (or symstore will use the uncompressed file)
for /F %%f in (%~2) do ((makecab %%f /L %4) & (del %%f))

rem store the compressed files to the symbol store
%1 add /y %3 /g %4 /t %5 /s %6 /o
