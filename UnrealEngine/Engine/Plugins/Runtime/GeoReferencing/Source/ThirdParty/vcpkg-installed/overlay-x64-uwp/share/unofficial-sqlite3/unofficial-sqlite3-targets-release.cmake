#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unofficial::sqlite3::sqlite3" for configuration "Release"
set_property(TARGET unofficial::sqlite3::sqlite3 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(unofficial::sqlite3::sqlite3 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/sqlite3.lib"
  )

list(APPEND _cmake_import_check_targets unofficial::sqlite3::sqlite3 )
list(APPEND _cmake_import_check_files_for_unofficial::sqlite3::sqlite3 "${_IMPORT_PREFIX}/lib/sqlite3.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
