=== BUILDING Flite ===
The original Flite source code has been left completely in tact. This is to allow easier porting of Flite into UE for the future. Custom bash scripts and CMake scripts have been added to the source tree to allow building for UE. The added files are as follow: 
1. CMakeLists.txt in the main Flite directory, Flite/src and all its subdirectories and Flite/lang and its subdirectories
2. setup_voicelist_and_langlist.sh in the Flite directory to create files generated from Flite's regular make process.  

For future versions of Flite, the following must be done: 
1. The CMakeLists.txt files in the old source tree must be copied to the new source trees in all above directories. 
2. The setup_voicelist_and_langlist.sh must be copied into the Flite directory
3. setup_voicelist_and_langlist.sh must be run in unix terminal or Windows WSL to generate the 2 files necessary in Flite/main  

The CMakeLists.txt file in the main Flite directory will build all necessary files to generate a monolithic static library that will be used in UE. Note that we intentionally exclude certain files from the library to support cross compilation as some Flite files include unix headers.  

