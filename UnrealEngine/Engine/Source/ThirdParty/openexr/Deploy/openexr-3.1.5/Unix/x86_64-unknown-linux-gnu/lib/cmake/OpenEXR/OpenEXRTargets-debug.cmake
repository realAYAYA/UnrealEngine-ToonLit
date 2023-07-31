#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenEXR::Iex" for configuration "Debug"
set_property(TARGET OpenEXR::Iex APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::Iex PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libIex-3_1_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::Iex )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::Iex "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libIex-3_1_d.a" )

# Import target "OpenEXR::IlmThread" for configuration "Debug"
set_property(TARGET OpenEXR::IlmThread APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::IlmThread PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libIlmThread-3_1_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::IlmThread )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::IlmThread "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libIlmThread-3_1_d.a" )

# Import target "OpenEXR::OpenEXRCore" for configuration "Debug"
set_property(TARGET OpenEXR::OpenEXRCore APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::OpenEXRCore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libOpenEXRCore-3_1_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::OpenEXRCore )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::OpenEXRCore "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libOpenEXRCore-3_1_d.a" )

# Import target "OpenEXR::OpenEXR" for configuration "Debug"
set_property(TARGET OpenEXR::OpenEXR APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::OpenEXR PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libOpenEXR-3_1_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::OpenEXR )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::OpenEXR "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libOpenEXR-3_1_d.a" )

# Import target "OpenEXR::OpenEXRUtil" for configuration "Debug"
set_property(TARGET OpenEXR::OpenEXRUtil APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenEXR::OpenEXRUtil PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libOpenEXRUtil-3_1_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::OpenEXRUtil )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::OpenEXRUtil "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libOpenEXRUtil-3_1_d.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
