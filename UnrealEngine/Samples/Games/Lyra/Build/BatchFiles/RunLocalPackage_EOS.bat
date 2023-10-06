pushd "%~dp0..\..\..\..\.."
call .\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -nop4 -project=./Samples/Games/Lyra/Lyra.uproject -cook -stage -archive -archivedirectory=./Samples/Games/Lyra/PackagedDevEOS -package -compressed -pak -prereqs -targetplatform=Win64 -build -target=LyraGameEOS -clientconfig=Development -utf8output -compile
pause

