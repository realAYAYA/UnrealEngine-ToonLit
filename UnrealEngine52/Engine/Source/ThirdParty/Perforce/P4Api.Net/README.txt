
                     Perforce .NET API Source Distribution

===============================================================================

Directory Structure:

p4api.net-src                     
|- bin                            Files generated during build
|  |- Debug                       Debug build files
|  |- Release                     Release build files
|- doc                            Usage documentation
|- examples                       Sample applications
|  |- bin                         Files generated during sample app. build
|  |  |- Debug                    Sample app. debug build files
|  |  |- Release                  Sample app. release build files
|  |- sln-bld-cmd                 Command line sample app.
|  |- sln-bld-gui                 Graphical sample app.
|- p4api                          For Perforce C++ API include and lib dirs.
|- p4api.net                      C# .NET API
|- p4api.net-unit-test            C# .NET API unit tests
|- p4bridge                       C++ .NET API bridge
|- p4bridge-unit-test             C++ .NET API bridge unit tests

===============================================================================

Building the .NET API:

SSL support
-----------

Perforce Server 2012.1 supports SSL connections and the C++ API has 
been compiled with this support. For applications that do not require
SSL support the C++ API provides a stub library (libp4sslstub.lib) 
to fulfill the linker requirements.

P4API.NET will build with the stub library (libp4sslstub.lib) by default
To enable SSL support, acquire the open SSL libraries (ssleay32.lib
and libeay32.lib) and place them in the p4api/lib directory and change
the Additional Dependencies for p4bridge and p4bridge-unit-test under
Linker -> Input to include the open SSL libraries (ssleay32.lib and
libeay32.lib) and remove the references to libp4sslstub.lib.


1) Download the Perforce C++ API build from the Perforce FTP site at
   "ftp://ftp.perforce.com/perforce/<rel>/bin.ntx86/p4api_vs2010_static.zip".
   <rel> should be the directory that corresponds to the same release of this
   .NET API. Use the API from bin.ntx64 to build 64-bit binaries.

2) Extract the Perforce C++ API.

3) Copy the "include" and "lib" directories from the Perforce C++ API directory
   into the "p4api" directory.

4) Open the .NET API solution with Visual Studio 2010.

5) Build the solution by right clicking on the solution and selecting "Build
   Solution" from the menu.

6) The newly built binaries will be in the "Debug" or "Release" subdirectory
   of the "bin" directory depending on the selected configuration.

NOTES:

The release configuration builds only the C# .NET API DLL and the C++ .NET API
bridge DLL.

To build the help file project (p4api.net.shfbproj) you will need the 
Sandcastle Help File Builder from http://shfb.codeplex.com/.

===============================================================================

Running the Unit Tests:

Before running any unit tests a Perforce server at version 2011.1 or higher 
must be placed in the system's PATH environment variable.

The C# .NET API unit tests run in the framework provided with Visual Studio
2010. To run the tests click the "Test" menu, click "Run", and then click
"All Tests in Solution".

The C++ .NET API bridge unit tests compile into a command line app that can be 
run from the command line. To run the tests open a command prompt. In the
command prompt window navigate to "bin\Debug" directory. Run 
"p4bridge-unit-test.exe".

NOTES:

Both unit tests use self-extracting zip files to deploy an ASCII server, a
Unicode server, and a sever running at security level 3 into the "C:\MyTestDir"
directory. These self-extracting zip files are located in the "bin\Debug"
directory and are named "a.exe", "u.exe", and "s3.exe", respectively.

===============================================================================

Building the Example Applications:

1) Open the example applications solution with Visual Studio 2010.

2) Confirm that the build platform is set to the correct configuration 
   ("x86" or "x64").

3) Build the example applications by right clicking on the solution and 
   selecting "Build Solution" from the menu.

4) The newly built applications will be in the "Debug" or "Release" 
   subdirectory of the "examples\bin" directory depending on the selected 
   configuration.

NOTES:

sln-bld-cmd.exe is a console application that builds a solution from a Perforce
depot. For usage run "sln-bld-cmd.exe /?". Builds are made in a directory named 
with a timestamp below the current working directory.

sln-bld-gui.exe is a Windows form application that builds a solution from a 
Perforce depot. Host, port, user, depot path of the solution, target build
directory, and location of "MSBuild.exe" are all required. Builds are made in a 
directory named with a timestamp below the specified target directory. Once a 
depot path is defined the application will check for changes submitted to that
location in the depot on a build interval which can be defined by the dropdown 
control. The default is 2 minutes.

Both applications creates a temporary workspace named 
"p4apinet_solution_builder_sample_application_client" and delete this workspace
on completion of the sync of files to the local machine. The sync command 
forces resynchronization and does not update the server's knowledge of the file
sync state. 

Both "p4api.net.dll" and "p4bridge.dll" must be present in the applications'
directory for the applications to run.

===============================================================================

For changes between releases, please see the release notes: p4api.netnotes.txt
which can be found at www.perforce.com
