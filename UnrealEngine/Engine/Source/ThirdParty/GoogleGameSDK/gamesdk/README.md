# Android Game Development Kit (AGDK)

## Integrating the AGDK libraries in your game

Unless you need to compile AGDK from sources, it's recommended that you use the package with the pre-compiled library. You can download it on https://developer.android.com/games/agdk.

## Requirements

AGDK requires Python executable named "python". A supported version of Python is supplied at `prebuilts/python/PLATFORM_NAME/bin/python`. The easiest way is to create a symlink to that executable and put it into any directory that is in your PATH.

## Build AGDK

In order to build AGDK, this project must be initialized using the [*repo* tool](https://gerrit.googlesource.com/git-repo/).
On [Windows](https://gerrit.googlesource.com/git-repo/+/HEAD/docs/windows.md), we recommend running all commands involving *repo* with Git Bash, to avoid issues with symlinks.

```bash
mkdir android-games-sdk
cd android-games-sdk
repo init -u https://android.googlesource.com/platform/manifest -b android-games-sdk
```
Ninja binary must be in your PATH like below. Please replace PLATFORM_NAME with either linux-x86 for Linux or darwin-86 for MacOS or windows-x86 for Windows, and run the following command:

```bash
export PATH="$PATH:`pwd`/../prebuilts/ninja/PLATFORM_NAME"
```

### Build with locally installed SDK/NDK

If the Android SDK is already installed locally, then download only the AGDK source and build tools (~500Mb).

```bash
repo sync -c -j8 gamesdk
repo sync -c -j8 external/modp_b64 external/googletest external/nanopb-c external/protobuf
repo sync -c -j8 prebuilts/cmake/linux-x86 prebuilts/cmake/windows-x86 prebuilts/cmake/darwin-x86
```

Point the environment variable `ANDROID_HOME` to your local Android SDK (and `ANDROID_NDK`, if the ndk isn't in `ANDROID_HOME/ndk-bundle`).
Use the following gradle tasks to build the Game SDK with or without Tuning Fork:

```bash
cd gamesdk
# Build Swappy:
./gradlew packageLocalZip -Plibraries=swappy -PpackageName=local
# Build Swappy and Tuning Fork:
./gradlew packageLocalZip -Plibraries=swappy,tuningfork -PpackageName=localtf
```

### Build with specific prebuilt SDKs

Download the project along with specific versions of prebuilt Android SDK and NDK (~4GB).
First, download the core project and tools:

```bash
repo sync -c -j8 gamesdk
repo sync -c -j8 external/modp_b64 external/googletest external/nanopb-c external/protobuf
repo sync -c -j8 prebuilts/cmake/linux-x86 prebuilts/cmake/windows-x86 prebuilts/cmake/darwin-x86
```

Next, use the download script to get prebuilt SDKs and/or NDKs.

```bash
cd gamesdk
./download.sh
```

Finally, build AGDK using downloaded prebuilts.

```bash
cd gamesdk
# Build and package Swappy in a ZIP file:
ANDROID_HOME=`pwd`/../prebuilts/sdk ANDROID_NDK=`pwd`/../prebuilts/ndk/r20 ./gradlew packageLocalZip -Plibraries=swappy -PpackageName=local
# Build and package Swappy and Tuning Fork in a ZIP file:
ANDROID_HOME=`pwd`/../prebuilts/sdk ANDROID_NDK=`pwd`/../prebuilts/ndk/r20 ./gradlew packageLocalZip -Plibraries=swappy,tuningfork -PpackageName=localtf
```

### Build with all prebuilt SDKs

Download the whole repository with all available prebuilt Android SDKs and NDKs (~23GB).

```bash
repo sync -c -j8
```

Build static and dynamic libraries for several SDK/NDK pairs.

```bash
cd gamesdk
ANDROID_HOME=`pwd`/../prebuilts/sdk ./gradlew packageZip -Plibraries=swappy,tuningfork
```

### Build properties reference

The command lines presented earlier are a combination of a **build or packaging task**, and a set of **properties**.

**Build tasks** are:
* `build`: build the libraries with prebuilt SDK/NDK.
* `buildLocal`: build the libraries with your locally installed Android SDK and NDK.
* `buildUnity`: build the libraries with the (prebuilt) SDK/NDK for use in Unity.
* `buildAar`: build the libraries with prebuilt SDK/NDK for distribution in a AAR with prefab.

**Packaging tasks** are:
* `packageZip`: create a zip of the native libraries for distribution.
* `packageLocalZip`: create a zip with the libraries compiled with your locally installed Android SDK and NDK.
* `packageUnityZip`: create a zip for integration in Unity.
* `packageMavenZip`: create a zip with the native libraries in a AAR file in Prefab format and a pom file. You can also use `packageAar` to only get the AAR file.

**Properties** are:
* `-Plibraries=swappy,tuningfork`: comma-separated list of libraries to build (for packaging/build tasks).
* `-PpackageName=gamesdk`: the name of the package, for packaging tasks. Defaults to "gamesdk".
* `-PbuildType=Release`: the build type, "Release" (default) or "Debug".
* Sample related properties:
  * `-PincludeSampleSources`: if specified, build tasks will include in their output the sources of the samples of the libraries that are built.
  * `-PincludeSampleArtifacts`: if specified, build tasks will also compile the samples and projects related to the libraries, and include their resulting artifact in the packaged archive.
  * `-PskipSamplesBuild`: if specified and `-PincludeSampleArtifacts` is specified, this will skip the actual `gradle build` of the samples and projects related to the libraries. Use this when you want to check that the packaging works correctly and don't want to rebuild everything.

Here are some commonly used examples:
```bash
# All prebuilt SDKs, with sample sources:
ANDROID_HOME=`pwd`/../prebuilts/sdk ./gradlew packageZip -Plibraries=swappy,tuningfork -PpackageName=fullsdk -PincludeSampleSources

# All prebuilt SDKs, with sample sources and precompiled samples:
ANDROID_HOME=`pwd`/../prebuilts/sdk ./gradlew packageZip -Plibraries=swappy,tuningfork -PpackageName=fullsdk -PincludeSampleSources -PincludeSampleArtifacts

# Swappy or Swappy+TuningFork for Unity:
./gradlew buildUnity --Plibraries=swappy --PpackageName=swappyUnity
./gradlew buildUnity --Plibraries=swappy,tuningfork --PpackageName=unity

# Zips to upload AARs to Maven:
./gradlew packageMavenZip -Plibraries=swappy -PpackageName=fullsdk
./gradlew packageMavenZip -Plibraries=tuningfork -PpackageName=fullsdk
```

## Tests

```bash
./gradlew localUnitTests # Requires a connected ARM64 device to run
./gradlew localDeviceInfoUnitTests # No device required, tests are running on host
```

## Samples

Samples are classic Android projects, using CMake to build the native code. They are also all triggering the build of AGDK.

### Using Grade command line:

```bash
cd samples/bouncyball && ./gradlew assemble
cd samples/cube && ./gradlew assemble
cd samples/tuningfork/insightsdemo && ./gradlew assemble
cd samples/tuningfork/experimentsdemo && ./gradlew assemble
```

The Android SDK/NDK exposed using environment variables (`ANDROID_HOME`) will be used for building both the sample project and AGDK.

### Using Android Studio

Open projects using Android Studio:

* `samples/bouncyball`
* `samples/cube`
* `samples/tuningfork/insightsdemo`
* `samples/tuningfork/experimentsdemo`

and run them directly (`Shift + F10` on Linux, `Control + R` on macOS). The local Android SDK/NDK (configured in Android Studio) will be used for building both the sample project and AGDK.

#### Development and debugging

After opening a sample project using Android Studio, uncomment the line containing `add_gamesdk_sources()`.
This will add the Swappy/Tuning Fork sources as part of the project. You can then inspect the source code (with working auto completions) and run the app in debug mode (with working breakpoints and inspectors).

#### Note for macOS Users

You may find that the APT demos will not build on your machine because protoc is being blocked by
security settings. A warning dialog message is displayed:
```"protoc" cannot be opened because the developer cannot be verified"```

In this case, please follow the instructions in the following article to allow protoc to be run:
https://support.apple.com/en-gb/guide/mac-help/mh40616/mac
