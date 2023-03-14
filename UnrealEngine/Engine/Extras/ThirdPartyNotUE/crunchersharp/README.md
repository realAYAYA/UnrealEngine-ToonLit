# crunchersharp
Program analyses debugger information file (PDB, so Microsoft Visual C++ only) and presents info about user defined structures (size, padding, cachelines, functions etc). 

- You can filter by namespace, search for a specific symbol
- You can import a .csv with the instance count to get the total waste, the format should be "Class Name, Number of instances"
- You can compare two PDBs
- You search for useless vtables, useless virtual...  

Original blog post: http://msinilo.pl/blog/?p=425
# Getting Started

### Windows 10 Visual Studio 2019

1. Open command prompt or PowerShell **as Admin** 
2. Find the directory where your `msdia` dll is located, which by default is loacted with you Visual Studio installation:

  ```
  cd C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\IDE
  ```
3. Manually register the DLL by typing "regsvr32 msdia<VERSION>.dll" (e.g. "regsvr32 msdia140.dll" for Visual Studio 2019)

4. Build the MSVC Project that you want with **full** debug symbols. By default in VS 2019/2017 it is set to 
use `/debug:fast`, we need `/debug:full`. This setting is located in Visual Studio project settings at `Linker > Debugging > Generate Debug Info`.
 * To change this in UE projects you will need set the Unreal Build Tool flag called `bUseFastPDBLinking`.
  Currently (4.24) the default value is to be turned off, which is what you want for using Cruncher. 

5. Open up Cruncher in Visual Studio
6. Right click on the C# Project and select "Properties"
7. Go to "Debug" and Select `Enable native code debugging` at the bottom. 
  * Make sure you have "All Configurations" selected here to ensure you have it in both Release and Debug
8. Create an `x86` build configuration by clicking the Solution Platforms dropdown and selecting `Configuration Manager...` 
   then clicking on the `Active Solution Platform` and creating a new `x86` setting.
  * This has to be x86 due to the `msdia140` DLL, if you don't do this you may get unresolved symbols in Release modes. 
  
![Screenshot](Screenshot.png "Example screenshot")
