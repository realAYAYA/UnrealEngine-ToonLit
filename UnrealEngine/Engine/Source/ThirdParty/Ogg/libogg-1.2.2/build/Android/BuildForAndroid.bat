SET NDK_PROJECT_PATH=%CD%/jni
cd %NDK_PROJECT_PATH%
call %NDKROOT%/ndk-build APP_ABI="armeabi-v7a x86" APP_PLATFORM=android-19 APP_BUILD_SCRIPT=%NDK_PROJECT_PATH%/Android.mk
call %NDKROOT%/ndk-build APP_ABI="arm64-v8a x86_64" APP_PLATFORM=android-21 APP_BUILD_SCRIPT=%NDK_PROJECT_PATH%/Android.mk
copy /y obj\local\armeabi-v7a\libogg.a ..\..\..\lib\Android\ARMv7
copy /y obj\local\arm64-v8a\libogg.a ..\..\..\lib\Android\ARM64
copy /y obj\local\x86\libogg.a ..\..\..\lib\Android\x86
copy /y obj\local\x86_64\libogg.a ..\..\..\lib\Android\x64