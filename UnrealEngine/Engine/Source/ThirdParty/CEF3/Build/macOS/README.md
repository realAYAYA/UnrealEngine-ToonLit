The macOS build needs to be built on a macOS host, docker doesn't support using a macOS image (yet, ever?)

You need to meet the macOS system requirements to build, [details here](https://bitbucket.org/chromiumembedded/cef/wiki/BranchesAndBuilding.md#markdown-header-release-branches).

Currently (version 4430) CEF requires macOS 10.15.4 and above, XCode 12.2  with the 11.0 SDK installed

To run the build type "./cef_build.sh" and wait (a few hours). 

Variables you can use for the build:
* CEF_BRANCH  - the version to build, see https://bitbucket.org/chromiumembedded/cef/wiki/BranchesAndBuilding.md#markdown-header-release-branches 
* CEF_BUILD_DIR - the path to checkout CEF and build into. By default this will be build/cef_$CEF_BRANCH in the current directory.

Arguments to cef_build.sh:
* "skipsync" - don't sync to upstream CEF/Chrome code, just build use what you already have downloaded
* "skipbuild" - don't sync OR build CEF itself, just build the wrapper piece