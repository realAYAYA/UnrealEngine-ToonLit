#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenEXR::Iex" for configuration "Release"
set_property(TARGET OpenEXR::Iex APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenEXR::Iex PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/Iex-3_1.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::Iex )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::Iex "${_IMPORT_PREFIX}/VS2015/x64/lib/Iex-3_1.lib" )

# Import target "OpenEXR::IlmThread" for configuration "Release"
set_property(TARGET OpenEXR::IlmThread APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenEXR::IlmThread PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/IlmThread-3_1.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::IlmThread )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::IlmThread "${_IMPORT_PREFIX}/VS2015/x64/lib/IlmThread-3_1.lib" )

# Import target "OpenEXR::OpenEXRCore" for configuration "Release"
set_property(TARGET OpenEXR::OpenEXRCore APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenEXR::OpenEXRCore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/OpenEXRCore-3_1.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::OpenEXRCore )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::OpenEXRCore "${_IMPORT_PREFIX}/VS2015/x64/lib/OpenEXRCore-3_1.lib" )

# Import target "OpenEXR::OpenEXR" for configuration "Release"
set_property(TARGET OpenEXR::OpenEXR APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenEXR::OpenEXR PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/OpenEXR-3_1.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::OpenEXR )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::OpenEXR "${_IMPORT_PREFIX}/VS2015/x64/lib/OpenEXR-3_1.lib" )

# Import target "OpenEXR::OpenEXRUtil" for configuration "Release"
set_property(TARGET OpenEXR::OpenEXRUtil APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenEXR::OpenEXRUtil PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/VS2015/x64/lib/OpenEXRUtil-3_1.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenEXR::OpenEXRUtil )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenEXR::OpenEXRUtil "${_IMPORT_PREFIX}/VS2015/x64/lib/OpenEXRUtil-3_1.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
