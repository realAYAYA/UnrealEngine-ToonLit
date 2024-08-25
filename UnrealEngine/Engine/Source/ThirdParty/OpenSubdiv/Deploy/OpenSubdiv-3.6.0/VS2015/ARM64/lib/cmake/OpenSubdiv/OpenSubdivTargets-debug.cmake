#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenSubdiv::osdCPU_static" for configuration "Debug"
set_property(TARGET OpenSubdiv::osdCPU_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenSubdiv::osdCPU_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/VS2015/ARM64/lib/osdCPU_d.lib"
  )

list(APPEND _cmake_import_check_targets OpenSubdiv::osdCPU_static )
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdCPU_static "${_IMPORT_PREFIX}/VS2015/ARM64/lib/osdCPU_d.lib" )

# Import target "OpenSubdiv::osdGPU_static" for configuration "Debug"
set_property(TARGET OpenSubdiv::osdGPU_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenSubdiv::osdGPU_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/VS2015/ARM64/lib/osdGPU_d.lib"
  )

list(APPEND _cmake_import_check_targets OpenSubdiv::osdGPU_static )
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdGPU_static "${_IMPORT_PREFIX}/VS2015/ARM64/lib/osdGPU_d.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
