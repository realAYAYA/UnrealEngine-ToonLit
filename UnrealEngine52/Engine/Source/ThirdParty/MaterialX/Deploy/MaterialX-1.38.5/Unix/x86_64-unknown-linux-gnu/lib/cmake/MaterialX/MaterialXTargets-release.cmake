#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "MaterialXCore" for configuration "Release"
set_property(TARGET MaterialXCore APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MaterialXCore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXCore.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXCore )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXCore "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXCore.a" )

# Import target "MaterialXFormat" for configuration "Release"
set_property(TARGET MaterialXFormat APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MaterialXFormat PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXFormat.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXFormat )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXFormat "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXFormat.a" )

# Import target "MaterialXGenShader" for configuration "Release"
set_property(TARGET MaterialXGenShader APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MaterialXGenShader PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenShader.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenShader )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenShader "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenShader.a" )

# Import target "MaterialXGenGlsl" for configuration "Release"
set_property(TARGET MaterialXGenGlsl APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MaterialXGenGlsl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenGlsl.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenGlsl )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenGlsl "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenGlsl.a" )

# Import target "MaterialXGenOsl" for configuration "Release"
set_property(TARGET MaterialXGenOsl APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MaterialXGenOsl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenOsl.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenOsl )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenOsl "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenOsl.a" )

# Import target "MaterialXGenMdl" for configuration "Release"
set_property(TARGET MaterialXGenMdl APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(MaterialXGenMdl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenMdl.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS MaterialXGenMdl )
list(APPEND _IMPORT_CHECK_FILES_FOR_MaterialXGenMdl "${_IMPORT_PREFIX}/Unix/x86_64-unknown-linux-gnu/lib/libMaterialXGenMdl.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
