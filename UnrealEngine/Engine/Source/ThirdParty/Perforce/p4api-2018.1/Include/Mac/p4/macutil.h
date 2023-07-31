/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

#ifndef __MACUTIL_H__
#define __MACUTIL_H__

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

bool WindowServicesAvailable();
bool RunCommandInNewTerminalWithCFStrRef( CFStringRef command );
bool RunCommandInNewTerminal( const char * command );
char * CreateFullPathToApplicationBundle( const char * path );
bool PathIsAFile( const char * path );

#ifdef __cplusplus
};
#endif

#endif // __MACUTIL_H__

