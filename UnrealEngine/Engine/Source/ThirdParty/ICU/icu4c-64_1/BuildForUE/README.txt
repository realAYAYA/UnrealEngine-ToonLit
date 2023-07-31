=== BUILDING ICU ===
This is a modified version of the ICU source tree that has the following changes:
  - UProperty has been renamed to UCharProperty to avoid conflicts with the Unreal version of UProperty.
  - The u_setDataFileFunctions function has been added to allow ICU data to be loaded with the Unreal Filesystem (see urename.h, uclean.h, and umapfile.cpp).
  - The U_PLATFORM_HAS_GETENV macro has been added which can be defined to 0 in platform build scripts to disable code that calls getenv() for platforms that don't have this function.

These changes must be reapplied to any new versions of ICU, then, the header files found in source/common/unicode and source/i18n/unicode should be copied to include/unicode.

The CMakeLists.txt file found in this directory is able to build the minimal subset of ICU that Unreal uses, and should be preferred over the existing Makefiles.
The only exception to this is if you need to generate ICU data, as that requires a tool not compiled by our CMake file.

=== BUILDING DATA ===
We build several sets of ICU data that needs to be copied into Engine/Content/Internationalization.
The Data/BuildICUData.bat file can generate these data sets using the filters found in Data/Filters.

Note: Building ICU data requires that you have Python 3 installed and available on your PATH.

 1) Download a source build of ICU, and extract the zip, then delete source/data.
 2) Download the data zip for ICU, and extract the contents to source/data.
      Note: The data building process can fail if the path is too long. Extract ICU somewhere short, like D:\icu.
 3) Copy everything from the Data folder into the root of your ICU folder (eg, to D:\icu).
 4) Run BuildICUData.bat from the root of your ICU folder.
 5) Copy the built ICU data from icu-data to Engine/Content/Internationalization (these are used by packaged projects).
 6) Copy the ICU data folder from the "All" data set to directly under the Engine/Content/Internationalization (this is used by the editor).

=== UPDATING TIMEZONE DATA (adapted from https://unicode-org.github.io/icu/userguide/datetime/timezone/#icu4c-tz-update-with-drop-in-res-files-icu-54-and-newer) ===
The ICU timezone data from IANA (colloquially, tzdata) gets updated much more frequently than the monolithic CLDR data embedded in ICU.
To update this data in engine content in-place (for icu4c versions > 54.0):

1) Sync the latest tzdata from the icu-data repo (the most recent folder in https://github.com/unicode-org/icu-data/tree/master/tzdata/icunew)
	Note: we are specifically looking at the "44" (for ICU 4.4) folder inside of here.
2) Replace "metaZones.res", "timezoneTypes.res", "windowsZones.res", and "zoneinfo64.res" in each of the filtered data directories in Engine/Content/Internationalization (including the base version the editor uses) for your icu4c version with the copies from the repo's "le" (little-endian) folder.
	Note: these binary files can be used directly (as long as icu4c 54.0 or greater is being used) and do not need to be regenerated or filtered; each filtered content set receives the whole resource.

	ex:
		Engine/Content/Internationalization/All/icudt64l/(metaZones|timezoneTypes|windowsZones|zoneinfo64).res
		Engine/Content/Internationalization/icudt64l/(metaZones|timezoneTypes|windowsZones|zoneinfo64).res
		et al.