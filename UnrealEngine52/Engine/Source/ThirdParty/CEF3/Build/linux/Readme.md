The CEF linux build makes use of a docker environment to create the build, meaning you can use both Windows or Linux for the build. The build itself uses a sysroot so the resulting Linux binaries should be mostly agnostic to the Linux distribution they are both built and then run on.

## Prerequisites
1. A Docker install from https://www.docker.com/get-started
On Windows, Docker WSL support (Windows Subsystem for Linux), follow these instructions https://docs.docker.com/docker-for-windows/wsl/
2. A git client, any will work, GitHub Desktop is free and easy to use

## Building
1. Change Docker to using "Linux Containers" but right clicking on it in the systray and selecting "Switch to Linux containers...". If it has "Switch to Window containers" in the list instead then you are already in the right mode
2. Change to the "linux/" folder (the folder this file is in)
3. Run "build.bat" from the root folder for the clone and wait for it to complete. The build will take a couple hours at least for the first run.

4. After the first stage of the docker setup is complete you will be dropped into a bash command prompt. Run "./cef_build.sh" here to run the CEF build itself.
5. Once complete you will have a .zip file in your local folder with the built debug and release binaries for Linux

## Iterating on the build
The "build.bat" file contains the defines we use for the CEF build as well as the version of CEF we use
Check the CEF https://bitbucket.org/chromiumembedded/cef/wiki/BranchesAndBuilding.md page for a list of available CEF versions

The "patches/" folder contains git style diff patch files applied to the build AFTER syncing but before building. If you have a local patch to apply add it here

If the build fails then you can manually enter the docker build environment to run "./cef_build.sh" again or edit local files as needed. Run these commands to enter the docker: 
    docker start cef3_build
    docker attach cef3_build

VSCode ships with some useful Docker extensions, you can use these to edit the files in the docker container directly and run commands all within VSCode itself. Install the "Remote - Containers" extension and attach to the "cef3_build" container to make use of this.
