Google ARCore Update instructions:

Updating ARCore SDK
Updating the ARCore SDK is a slightly opaque process, so let's write it down to safe ourselves trouble in the future. The author doesn't know how all the knobs work here, and will explicitly document those unknowns here.


Components:
We embed portions of the ARCore SDK into our tree at Engine/Source/ThirdParty/GoogleARCore/

There are three primary components:

Build/metadata
GoogleARCoreSDK.build.cs
GoogleARCoreSDK.tps

include
arcore_c_api.h
This is the most important bit, and we get this from the Google ARCore Android SDK GitHub: https://github.com/google-ar/arcore-android-sdk/releases
arcore_ios_c_api.h
This seems deprecated, but it's still part of the tree.

lib
android-aar/com/google/ar/core/<VERSION>/core-<VERSION>.aar
This is the binary Android lib for ARCore. Currently, this seems to be distributed by Google Maven, Google's binary artifact repository (kinda like JFrog Artifactory). More on this below.
<ABI>/libarcore_sdk_c.so + <ABI>/libarcore_sdk_jni.so
These libs are extracted from the above .aar archive, and used by native applications (e.g. UE) to interact with the ARCore SDK.

Unreal side:
Engine/Plugins/Runtime/AR/Google/GoogleARCore/Source/GoogleARCoreBase/GoogleARCoreBase_APL.xml. The items in this file go into the unreal android build process, in particular there is a reference to the ARCore version number in here which is fed into gradle to tell it which version the build depends on.




Process:
The most important and difficult part is to fetch the header and libs needed to build against the ARCore SDK.

Header
You can fetch the header from the ARCore Android SDK GitHub, likely from the Releases Page.
In our case, we're going to grab the 1.26.0 SDK: https://github.com/google-ar/arcore-android-sdk/releases/download/v1.26.0/arcore-android-sdk-1.26.0.zip
Once we unzip, we can navigate to libraries/include/arcore_c_api.h, and we have our new SDK header!

Libs
But what about the libs? Why aren't they just INCLUDED in the SDK zip?
Some of the information about how to add ARCore as a build dependency is included in the ARCore documentation: https://developers.google.com/ar/develop/c/enable-arcore#dependencies
The drawback here is that this doc expects that you know how Gradle, Maven, and Android Studio work. Which...might not be true. If we read the docs, and follow the steps, we can actually extract the various .so files via the Gradle build system. These directions are actually embedded in to the native samples included in the SDK. However, these instructions do not retain the .aar file. Technically, I don't think we need the .aar file, but I'm guessing we keep it around as the source of truth, in case Google is sneaky and updates the artifact.
The ARCore documentation basically says use Gradle to download the .aar from Maven, and then extract the .so libs from the archive. Well, we can do that too!
Here are the listings of ARCore releases on Maven: https://maven.google.com/web/index.html#com.google.ar:core
Here is the 1.26.0 release: https://maven.google.com/web/index.html#com.google.ar:core:1.26.0
And here is the .aar we want to download: https://dl.google.com/android/maven2/com/google/ar/core/1.26.0/core-1.26.0.aar
From there, we can open the jni/ folder (7Zip can open/extract the .aar) and grab the SOs that we need!

GoogleARCoreBase_APL.xml
Update the version number in: "implementation('com.google.ar:core:1.26.0')".
In theory there could also be specific changes required by a new version.  permissions, etc.

Of course you may also need to update the actual unreal integration to stop using deprecated or removed ARCore APIs, etc.




Ways it has gone wrong before:
Runtime errors about ARCore SDK functions not found in libUnreal.so: This may mean your ARCore .so files are missing or that you have the wrong version.  In once case I had not updated GoogleARCoreBase_APL.xml and gradle found the older version somewhere (probably downloaded it during the build) so my application was failing on one of the functions new in my updated ARCore version.