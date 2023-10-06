pushd "%~dp0..\..\..\..\.."
call .\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -nop4 -project=./Samples/Games/Lyra/Lyra.uproject -cook -stage -archive -archivedirectory=./Samples/Games/Lyra/PackagedDev -package -compressed -pak -prereqs -targetplatform=Win64 -build -target=LyraGame -clientconfig=Development -utf8output -compile
pause

