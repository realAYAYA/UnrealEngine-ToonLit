#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unofficial::sqlite3::sqlite3" for configuration "Debug"
set_property(TARGET unofficial::sqlite3::sqlite3 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(unofficial::sqlite3::sqlite3 PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/sqlite3.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/sqlite3.dll"
  )

list(APPEND _cmake_import_check_targets unofficial::sqlite3::sqlite3 )
list(APPEND _cmake_import_check_files_for_unofficial::sqlite3::sqlite3 "${_IMPORT_PREFIX}/debug/lib/sqlite3.lib" "${_IMPORT_PREFIX}/debug/bin/sqlite3.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
