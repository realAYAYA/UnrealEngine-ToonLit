Chromium Embedded Framework (CEF) Standard Binary Distribution for MacOS
-------------------------------------------------------------------------------

Date:             June 30, 2022

CEF Version:      90.6.7+g19ba721+chromium-90.0.4430.212
CEF URL:          https://bitbucket.org/chromiumembedded/cef.git
                  @19ba7216a0d852845bda0d13578435f53458e65a

Chromium Version: 90.0.4430.212
Chromium URL:     https://chromium.googlesource.com/chromium/src.git
                  @f8afce18734a8939310459c13c80c4b422eb64c7

This distribution contains all components necessary to build and distribute an
application using CEF on the MacOS platform. Please see the LICENSING
section of this document for licensing terms and conditions.


CONTENTS
--------

cmake       Contains CMake configuration files shared by all targets.

Debug       Contains the "Chromium Embedded Framework.framework" and other
            components required to run the debug version of CEF-based
            applications.

include     Contains all required CEF header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains the "Chromium Embedded Framework.framework" and other
            components required to run the release version of CEF-based
            applications.

tests/      Directory of tests that demonstrate CEF usage.

  cefclient Contains the cefclient sample application configured to build
            using the files in this distribution. This application demonstrates
            a wide range of CEF functionalities.

  cefsimple Contains the cefsimple sample application configured to build
            using the files in this distribution. This application demonstrates
            the minimal functionality required to create a browser window.

  ceftests  Contains unit tests that exercise the CEF APIs.

  gtest     Contains the Google C++ Testing Framework used by the ceftests
            target.

  shared    Contains source code shared by the cefclient and ceftests targets.


USAGE
-----

Building using CMake:
  CMake can be used to generate project files in many different formats. See
  usage instructions at the top of the CMakeLists.txt file.

Please visit the CEF Website for additional usage information.

https://bitbucket.org/chromiumembedded/cef/


REDISTRIBUTION
--------------

This binary distribution contains the below components. Components listed under
the "required" section must be redistributed with all applications using CEF.
Components listed under the "optional" section may be excluded if the related
features will not be used.

Applications using CEF on OS X must follow a specific app bundle structure.
Replace "cefclient" in the below example with your application name.

cefclient.app/
  Contents/
    Frameworks/
      Chromium Embedded Framework.framework/
        Chromium Embedded Framework <= main application library
        Libraries/
          libEGL.dylib <= angle support libraries
          libGLESv2.dylib <=^
          libswiftshader_libEGL.dylib <= swiftshader support libraries
          libswiftshader_libGLESv2.dylib <=^
        Resources/
          chrome_100_percent.pak <= non-localized resources and strings
          chrome_200_percent.pak <=^
          resources.pak          <=^
          icudtl.dat <= unicode support
          snapshot_blob.bin, v8_context_snapshot.[x86_64|arm64].bin <= V8 initial snapshot
          en.lproj/, ... <= locale-specific resources and strings
          Info.plist
      cefclient Helper.app/
        Contents/
          Info.plist
          MacOS/
            cefclient Helper <= helper executable
          Pkginfo
      Info.plist
    MacOS/
      cefclient <= cefclient application executable
    Pkginfo
    Resources/
      binding.html, ... <= cefclient application resources

The "Chromium Embedded Framework.framework" is an unversioned framework that
contains CEF binaries and resources. Executables (cefclient, cefclient Helper,
etc) must load this framework dynamically at runtime instead of linking it
directly. See the documentation in include/wrapper/cef_library_loader.h for
more information.

The "cefclient Helper" app is used for executing separate processes (renderer,
plugin, etc) with different characteristics. It needs to have a separate app
bundle and Info.plist file so that, among other things, it doesn't show dock
icons.

Required components:

The following components are required. CEF will not function without them.

* CEF core library.
  * Chromium Embedded Framework.framework/Chromium Embedded Framework

* Unicode support data.
  * Chromium Embedded Framework.framework/Resources/icudtl.dat

* V8 snapshot data.
  * Chromium Embedded Framework.framework/Resources/snapshot_blob.bin
  * Chromium Embedded Framework.framework/Resources/v8_context_snapshot.bin

Optional components:

The following components are optional. If they are missing CEF will continue to
run but any related functionality may become broken or disabled.

* Localized resources.
  Locale file loading can be disabled completely using
  CefSettings.pack_loading_disabled.

  * Chromium Embedded Framework.framework/Resources/*.lproj/
    Directory containing localized resources used by CEF, Chromium and Blink. A
    .pak file is loaded from this directory based on the CefSettings.locale
    value. Only configured locales need to be distributed. If no locale is
    configured the default locale of "en" will be used. Without these files
    arbitrary Web components may display incorrectly.

* Other resources.
  Pack file loading can be disabled completely using
  CefSettings.pack_loading_disabled.

  * Chromium Embedded Framework.framework/Resources/chrome_100_percent.pak
  * Chromium Embedded Framework.framework/Resources/chrome_200_percent.pak
  * Chromium Embedded Framework.framework/Resources/resources.pak
    These files contain non-localized resources used by CEF, Chromium and Blink.
    Without these files arbitrary Web components may display incorrectly.

* Angle support.
  * Chromium Embedded Framework.framework/Libraries/libEGL.dylib
  * Chromium Embedded Framework.framework/Libraries/libGLESv2.dylib
  Without these files HTML5 accelerated content like 2D canvas, 3D CSS and WebGL
  will not function.

* SwiftShader support.
  * Chromium Embedded Framework.framework/Libraries/libswiftshader_libEGL.dylib
  * Chromium Embedded Framework.framework/Libraries/libswiftshader_libGLESv2.dylib
  Without these files WebGL will not function in software-only mode when the GPU
  is not available or disabled.


LICENSING
---------

The CEF project is BSD licensed. Please read the LICENSE.txt file included with
this binary distribution for licensing terms and conditions. Other software
included in this distribution is provided under other licenses. Please visit
"about:credits" in a CEF-based application for complete Chromium and third-party
licensing information.
