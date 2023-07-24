pushd "%~dp0..\..\..\..\.."
call .\Engine\Build\BatchFiles\RunUAT.bat BuildGraph -Script=Samples/Games/Lyra/Build/LyraTests.xml -Target="BuildAndTest Lyra" -UseLocalBuildStorage
pause

