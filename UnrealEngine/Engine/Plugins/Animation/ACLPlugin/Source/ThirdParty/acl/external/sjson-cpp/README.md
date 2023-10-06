[![CLA assistant](https://cla-assistant.io/readme/badge/nfrechette/sjson-cpp)](https://cla-assistant.io/nfrechette/sjson-cpp)
[![All Contributors](https://img.shields.io/github/all-contributors/nfrechette/sjson-cpp)](#contributors-)
[![Build status](https://ci.appveyor.com/api/projects/status/oynd3x3d9umjaruf/branch/develop?svg=true)](https://ci.appveyor.com/project/nfrechette/sjson-cpp)
[![Build status](https://github.com/nfrechette/sjson-cpp/workflows/build/badge.svg)](https://github.com/nfrechette/sjson-cpp/actions)
[![Sonar Status](https://sonarcloud.io/api/project_badges/measure?project=nfrechette_sjson-cpp&metric=alert_status)](https://sonarcloud.io/dashboard?id=nfrechette_sjson-cpp)
[![GitHub (pre-)release](https://img.shields.io/github/release/nfrechette/sjson-cpp/all.svg)](https://github.com/nfrechette/sjson-cpp/releases)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/nfrechette/sjson-cpp/master/LICENSE)

# sjson-cpp

`sjson-cpp` is a C++ library to read and write [Simplified JSON](http://help.autodesk.com/view/Stingray/ENU/?guid=__stingray_help_managing_content_sjson_html) files.
It aims to be minimal, fast, and get out of the way of the programmer.

By design, the library does no memory allocations. This is in contrast to the [nflibs C parser](https://github.com/niklasfrykholm/nflibs).

Everything is **100% C++11** header based for easy and trivial integration.

This parser is intended to accept only pure SJSON, and it will fail if given a JSON file, unlike the [Autodesk JS Stingray parser](https://github.com/Autodesk/sjson).

## The SJSON format

The data format is described [here](http://help.autodesk.com/view/Stingray/ENU/?guid=__stingray_help_managing_content_sjson_html) in the Stingray documentation.

TODO: Add a reference sjson file showing the format as a form of loose specification

## Unicode support

UTF-8 support is as follow:

*  String values return a raw `StringView` into the SJSON buffer. It is the responsability of the caller to interpret it as ANSI or UTF-8.
*  String values properly support escaped unicode sequences in that they are returned raw in the `StringView`.
*  Keys do not support UTF-8, they must be ANSI.
*  The BOM is properly skipped if present

Unicode formats other than UTF-8 aren't supported.

## Supported platforms

*  Windows VS2015 x86 and x64
*  Windows (VS2017, VS2019) x86, x64, and ARM64
*  Windows VS2019 with clang9 x86 and x64
*  Linux (gcc 5 to 10) x86 and x64
*  Linux (clang 4 to 11) x86 and x64
*  OS X (Xcode 10.3, 11.7) x86 and x64
*  Android (NDK 21) ARMv7-A and ARM64
*  iOS (Xcode 10.3) ARM64
*  Emscripten (1.39.11) WASM

The above supported platform list is only what is tested every release but if it compiles, it should run just fine.

Notes: *VS2017* and *VS2019* compile with *ARM64* on *AppVeyor* but I have no device to test them with. Xcode 11 no longer supports x86.

## External dependencies

There are none! You don't need anything else to get started: everything is self contained.
See [here](./external) for details.

## Getting up and running

This library is **100%** headers as such you just need to include them in your own project to start using it. However, if you wish to run the unit tests you will need a few things, see below.

### Windows, Linux, and OS X for x86 and x64

1. Install *CMake 3.2* or higher (*3.14* for Visual Studio 2019, or *3.10* on OS X with *Xcode 10*), *Python 2.7 or 3*, and the proper compiler for your platform.
2. Execute `git submodule update --init` to get the files of external submodules (e.g. Catch2).
3. Generate the IDE solution with: `python make.py`  
   The solution is generated under `./build`
4. Build the IDE solution with: `python make.py -build`
5. Run the unit tests with: `python make.py -unit_test`

### Windows ARM64

For *Windows on ARM64*, the steps are identical to *x86 and x64* but you will need *CMake 3.13 or higher* and you must provide the architecture on the command line: `python make.py -compiler vs2017 -cpu arm64`

### Android

For *Android*, the steps are identical to *Windows, Linux, and OS X* but you also need to install *Android NDK 21* (or higher). The build uses `gradle` and `-unit_test` will deploy and run on the device when executed (make sure that the `adb` executable is in your `PATH` for this to work).

*Android Studio v3.5* can be used to launch and debug. After running *CMake* to build and generate everything, the *Android Studio* projects can be found under the `./build` directory.

### iOS

For *iOS*, the steps are identical to the other platforms but due to code signing, you will need to perform the builds from *Xcode* manually. Note that this is only an issue if you attempt to use the tools or run the unit tests locally.

### Emscripten

Emscripten support currently only has been tested on OS X and Linux. To use it, make sure to install a recent version of Emscripten SDK 1.39.11+.

## Commit message format

This library uses the [angular.js message format](https://github.com/angular/angular.js/blob/master/DEVELOPERS.md#commits) and it is enforced with commit linting through every pull request.

## Authors

*  [Nicholas Frechette](https://github.com/nfrechette)
*  [Cody Jones](https://github.com/CodyDWJones)

## License, copyright, and code of conduct

This project uses the [MIT license](LICENSE).

Copyright (c) 2017 Nicholas Frechette, Cody Jones, and sjson-cpp contributors

Please note that this project is released with a [Contributor Code of Conduct](CODE_OF_CONDUCT.md). By participating in this project you agree to abide by its terms.

## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tr>
    <td align="center"><a href="https://github.com/CodyDWJones"><img src="https://avatars.githubusercontent.com/u/28773740?v=4?s=100" width="100px;" alt=""/><br /><sub><b>CodyDWJones</b></sub></a><br /><a href="https://github.com/nfrechette/sjson-cpp/commits?author=CodyDWJones" title="Code">💻</a> <a href="#maintenance-CodyDWJones" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/janisozaur"><img src="https://avatars.githubusercontent.com/u/550290?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Michał Janiszewski</b></sub></a><br /><a href="https://github.com/nfrechette/sjson-cpp/commits?author=janisozaur" title="Code">💻</a> <a href="#maintenance-janisozaur" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/tirpidz"><img src="https://avatars.githubusercontent.com/u/9991876?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Martin Turcotte</b></sub></a><br /><a href="#maintenance-tirpidz" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/Meradrin"><img src="https://avatars.githubusercontent.com/u/7066278?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Meradrin</b></sub></a><br /><a href="https://github.com/nfrechette/sjson-cpp/issues?q=author%3AMeradrin" title="Bug reports">🐛</a></td>
  </tr>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind welcome!