SET NDK_PROJECT_PATH=%CD%
call %NDKROOT%/ndk-build APP_ABI="armeabi-v7a x86" APP_PLATFORM=android-19 APP_BUILD_SCRIPT=%CD%/Android.mk
call %NDKROOT%/ndk-build APP_ABI="arm64-v8a x86_64" APP_PLATFORM=android-21 APP_BUILD_SCRIPT=%CD%/Android.mk
copy obj\local\armeabi-v7a\*.a Android\ARMv7
copy obj\local\arm64-v8a\*.a Android\ARM64
copy obj\local\x86\*.a Android\x86
copy obj\local\x86_64\*.a Android\x64