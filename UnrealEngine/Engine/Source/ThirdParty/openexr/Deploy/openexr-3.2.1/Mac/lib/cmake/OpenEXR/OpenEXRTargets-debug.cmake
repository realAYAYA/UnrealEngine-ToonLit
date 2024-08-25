#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenEXR::Iex" for configuration "Debug"
set_property(TARGET OpenEXR::Iex APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::Iex PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Mac/lib/libIex-3_2_d.a"
  )

list(APPEND _cmake_import_check_targets OpenEXR::Iex )
list(APPEND _cmake_import_check_files_for_OpenEXR::Iex "${_IMPORT_PREFIX}/Mac/lib/libIex-3_2_d.a" )

# Import target "OpenEXR::IlmThread" for configuration "Debug"
set_property(TARGET OpenEXR::IlmThread APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::IlmThread PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Mac/lib/libIlmThread-3_2_d.a"
  )

list(APPEND _cmake_import_check_targets OpenEXR::IlmThread )
list(APPEND _cmake_import_check_files_for_OpenEXR::IlmThread "${_IMPORT_PREFIX}/Mac/lib/libIlmThread-3_2_d.a" )

# Import target "OpenEXR::OpenEXRCore" for configuration "Debug"
set_property(TARGET OpenEXR::OpenEXRCore APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::OpenEXRCore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Mac/lib/libOpenEXRCore-3_2_d.a"
  )

list(APPEND _cmake_import_check_targets OpenEXR::OpenEXRCore )
list(APPEND _cmake_import_check_files_for_OpenEXR::OpenEXRCore "${_IMPORT_PREFIX}/Mac/lib/libOpenEXRCore-3_2_d.a" )

# Import target "OpenEXR::OpenEXR" for configuration "Debug"
set_property(TARGET OpenEXR::OpenEXR APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::OpenEXR PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Mac/lib/libOpenEXR-3_2_d.a"
  )

list(APPEND _cmake_import_check_targets OpenEXR::OpenEXR )
list(APPEND _cmake_import_check_files_for_OpenEXR::OpenEXR "${_IMPORT_PREFIX}/Mac/lib/libOpenEXR-3_2_d.a" )

# Import target "OpenEXR::OpenEXRUtil" for configuration "Debug"
set_property(TARGET OpenEXR::OpenEXRUtil APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::OpenEXRUtil PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Mac/lib/libOpenEXRUtil-3_2_d.a"
  )

list(APPEND _cmake_import_check_targets OpenEXR::OpenEXRUtil )
list(APPEND _cmake_import_check_files_for_OpenEXR::OpenEXRUtil "${_IMPORT_PREFIX}/Mac/lib/libOpenEXRUtil-3_2_d.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
