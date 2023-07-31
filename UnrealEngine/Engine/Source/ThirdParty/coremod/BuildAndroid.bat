cd coremod-4.2.6
set NDK_PROJECT_PATH=%CD%
call %NDKROOT%\ndk-build APP_ABI="armeabi-v7a x86" APP_PLATFORM=android-19
call %NDKROOT%\ndk-build APP_ABI="arm64-v8a x86_64" APP_PLATFORM=android-21
copy obj\local\armeabi-v7a\libxmp-coremod.a lib\Android\armeabi-v7a
copy obj\local\arm64-v8a\libxmp-coremod.a lib\Android\arm64-v8a
copy obj\local\x86\libxmp-coremod.a lib\Android\x86
copy obj\local\x86_64\libxmp-coremod.a lib\Android\x64
