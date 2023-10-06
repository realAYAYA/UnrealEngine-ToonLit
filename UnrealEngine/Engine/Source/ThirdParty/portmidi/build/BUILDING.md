# Summary

We don't have to build this library often. But when we do, we need to unzip the TPS approved version (/Engine/Source/ThirdParty/portmidi/build/portmidi-src-217.zip) and update it to work.

# Notes

The last time we did this, we had to:

- Write a BuildForMac.command script
- Install a JDK (either from Oracle, or using `brew`)
- Modify pm\_common/CMakeLists.txt to
  - Find JDK headers
  - Link against the JavaNativeFoundation.framework (instead of JavaVM.framework)
- Modify/update pm\_mac/pm\_mac.xcodeproj
  - Lots of Xcode project file related updates, all done by Xcode itself.
- Manually copy the resulting library from /usr/local/lib/libportmidi\_s.a to lib/Mac/libportmidi.a

These modifications were then archived as portmidi-src-217-xcode-12\_4.tgz
