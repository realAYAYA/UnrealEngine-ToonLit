1) You need to have a licensed version of Solidworks to build this plugin

The solidowrks DLLs needed are the following:
SolidWorks.Interop.sldworks.dll
SolidWorks.Interop.swcommands.dll
SolidWorks.Interop.swconst.dll
SolidWorks.Interop.swmotionstudy.dll
SolidWorks.Interop.swpublished.dll
solidworkstools.dll

2) Build the DatasmithSolidorks project

3) Signing
[TBD]

4) Find the installer executable in

Engine\Source\Programs\Enterprise\Datasmith\DatasmithSolidworksExporter\Installer\bin\Release\en-us

5) The installer procedure will create a folder in the predefined binaries directory:

Engine\Binaries\Win64\Solidworks

This directory should contain the final compiled installer, but at the time of this writing it will receive the compiled DLL instead (which is useless).

6) Modifying the project

Unreal is unaware of changes to the project. If files are added or removed from the Private folder, run generateprojectfiles.bat to update the Solution, but don't forget to manually modify the C# project as well
Engine\Source\Programs\Enterprise\Datasmith\DatasmithSolidworksExporter\DatasmithSolidworks\DatasmithSolidworks.csproj

Because Unreal is unaware of changes to these files (the C# build is not part of BuildTool's C++ based building process), you may have to invalidate the build by manually removing the following folders:
Engine\Binaries\Win64\Solidworks
Engine\Intermediate\Build\Win64\DatasmithSolidworks

7) VERSION
To change the version, modify the two relevant version properties properties in file
Engine\Source\Programs\Enterprise\Datasmith\DatasmithSolidworksExporter\DatasmithSolidworks\Properties\AssemblyInfo.cs
