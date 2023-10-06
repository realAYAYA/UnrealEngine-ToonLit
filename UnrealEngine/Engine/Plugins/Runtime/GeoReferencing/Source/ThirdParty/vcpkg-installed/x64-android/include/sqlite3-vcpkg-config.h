/*
 * This file was generated to inject vcpkg feature selections into the installed copy of
 * sqlite so that consumers need not get the values from pkgconfig or CMake configs.
 *
 * No include guard: intentionally reuses the include guard from sqlite3.h.
 */

#ifndef SQLITE_API
/* #undef SQLITE_API */
#endif

#define SQLITE_ENABLE_UNLOCK_NOTIFY 1
/* #undef SQLITE_ENABLE_FTS3 */
/* #undef SQLITE_ENABLE_FTS4 */
/* #undef SQLITE_ENABLE_FTS5 */
/* #undef SQLITE_ENABLE_MEMSYS3 */
/* #undef SQLITE_ENABLE_MEMSYS5 */
/* #undef SQLITE_ENABLE_MATH_FUNCTIONS */
/* #undef SQLITE_ENABLE_UPDATE_DELETE_LIMIT */
/* #undef SQLITE_ENABLE_RTREE */
/* #undef SQLITE_ENABLE_SESSION */
/* #undef SQLITE_ENABLE_PREUPDATE_HOOK */
/* #undef SQLITE_OMIT_LOAD_EXTENSION */
/* #undef SQLITE_ENABLE_GEOPOLY */
/* #undef SQLITE_ENABLE_JSON1 */
/* #undef SQLITE_OS_WIN */
/* #undef SQLITE_OS_WINRT */
#define SQLITE_ENABLE_COLUMN_METADATA 1
#define SQLITE_OS_UNIX 1
