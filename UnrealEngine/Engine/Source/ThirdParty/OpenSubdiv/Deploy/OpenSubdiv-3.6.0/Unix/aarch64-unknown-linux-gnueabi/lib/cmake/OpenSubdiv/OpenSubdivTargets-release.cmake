#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenSubdiv::osdCPU_static" for configuration "Release"
set_property(TARGET OpenSubdiv::osdCPU_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenSubdiv::osdCPU_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libosdCPU.a"
  )

list(APPEND _cmake_import_check_targets OpenSubdiv::osdCPU_static )
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdCPU_static "${_IMPORT_PREFIX}/Unix/aarch64-unknown-linux-gnueabi/lib/libosdCPU.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
