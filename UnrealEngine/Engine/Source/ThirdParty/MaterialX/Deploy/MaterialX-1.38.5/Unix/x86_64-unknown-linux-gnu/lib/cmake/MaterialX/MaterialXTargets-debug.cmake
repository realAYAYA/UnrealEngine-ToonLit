#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "MaterialXCore" for configuration "Debug"
set_property(TARGET MaterialXCore APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(MaterialXCore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXCore_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXCore )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXCore "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXCore_d.a" )

# Import target "MaterialXFormat" for configuration "Debug"
set_property(TARGET MaterialXFormat APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(MaterialXFormat PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXFormat_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXFormat )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXFormat "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXFormat_d.a" )

# Import target "MaterialXGenShader" for configuration "Debug"
set_property(TARGET MaterialXGenShader APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(MaterialXGenShader PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenShader_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenShader )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenShader "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenShader_d.a" )

# Import target "MaterialXGenGlsl" for configuration "Debug"
set_property(TARGET MaterialXGenGlsl APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(MaterialXGenGlsl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenGlsl_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenGlsl )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenGlsl "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenGlsl_d.a" )

# Import target "MaterialXGenOsl" for configuration "Debug"
set_property(TARGET MaterialXGenOsl APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(MaterialXGenOsl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenOsl_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenOsl )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenOsl "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenOsl_d.a" )

# Import target "MaterialXGenMdl" for configuration "Debug"
set_property(TARGET MaterialXGenMdl APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(MaterialXGenMdl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenMdl_d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenMdl )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenMdl "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenMdl_d.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
