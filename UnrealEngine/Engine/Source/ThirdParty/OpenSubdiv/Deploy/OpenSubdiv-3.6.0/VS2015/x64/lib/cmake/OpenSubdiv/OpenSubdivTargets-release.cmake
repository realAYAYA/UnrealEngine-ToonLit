#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenSubdiv::osdCPU_static" for configuration "Release"
set_property(TARGET OpenSubdiv::osdCPU_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenSubdiv::osdCPU_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/osdCPU.lib"
  )

list(APPEND _cmake_import_check_targets OpenSubdiv::osdCPU_static )
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdCPU_static "${_IMPORT_PREFIX}/VS2015/x64/lib/osdCPU.lib" )

# Import target "OpenSubdiv::osdGPU_static" for configuration "Release"
set_property(TARGET OpenSubdiv::osdGPU_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenSubdiv::osdGPU_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/osdGPU.lib"
  )

list(APPEND _cmake_import_check_targets OpenSubdiv::osdGPU_static )
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdGPU_static "${_IMPORT_PREFIX}/VS2015/x64/lib/osdGPU.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
