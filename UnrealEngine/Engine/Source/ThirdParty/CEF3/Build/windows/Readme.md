The CEF windows build makes use of a docker environment to create the build. Make sure you sure you have 150+ GB free disk space on a SSD.

## Prerequisites
1. A recent Docker install from https://www.docker.com/get-started
2. A git client, any will work, GitHub Desktop is free and easy to use

## Building
1. Change Docker to using "Windows Containers" but right clicking on it in the systray and selecting "Switch to Windows containers..."
If it has "Switch to Linux containers" in the list instead then you are already in the right mode
2. Change to the "windows/" folder (the folder this file is in)
3. Create a Docker volume called "cef3_code_volume" that will be used to host the Chromium sync and build. This is a once off operation.
Use the Docker Dashboard UI, under the "Volumes" tab to create a new volume called "cef3_code_volume" . You will need to have this volume located on a SSD drive, you can use the data-root configuration key in docker to change this location if needed.

        *(optionally)* Use the "docker volume create" command to make the volume instead.
4. Run "build.bat" from the root folder for the clone and wait for it to complete. 

5. Initially a base image containing the appropriate visual studio tools is created then an image for the CEF install itself is created and a build is started
*Doing a full build can take 48 hours or more*
6. Once complete you will have a .zip file in your local folder with the built debug and release binaries for Windows. Use the suggested "docker cp" command to extract the file locally.

## Iterating on the build
The "build.bat" file contains the defines we use for the CEF build as well as the version of CEF we use
Check the CEF https://bitbucket.org/chromiumembedded/cef/wiki/BranchesAndBuilding.md page for a list of available CEF versions
The "patches/" folder contains git style diff patch files applied to the build AFTER syncing but before building. If you have a local patch to apply add it here
If the build fails then you can manually enter the docker build environment to run "cef_build.bat" again or edit local files as needed. Run these commands to enter the docker: 
    docker start cef3_build
    docker attach cef3_build
VSCode ships with some useful Docker extensions, you can use these to edit the files in the docker container directly and run commands all within VSCode itself. Install the "Remote - Containers" extension and attach to the "cef3_build" container to make use of this.


## After Building
Use the "update_drop.bat" file located in Engine\Source\Thirdparty\CEF3 , pointing the 1st parameter of the command to the location you have extracted the package.zip above to.

If you are building a different version of CEF than the one shipped with your engine, be sure to update build.sh and CEF3.build.cs to point to the new version.
