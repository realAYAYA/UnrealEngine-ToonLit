// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(__GNUC__)
        #define DLL_EXPORT __attribute__((dllexport))
        #define DLL_IMPORT __attribute__((dllimport))
    #else
        #define DLL_EXPORT __declspec(dllexport)
        #define DLL_IMPORT __declspec(dllimport)
    #endif
#elif defined(__GNUC__)
    #define DLL_EXPORT __attribute__((visibility("default")))
    #define DLL_IMPORT DLL_EXPORT
#endif

#if defined(RL_BUILD_SHARED)
    // Build shared library
    #define DNAAPI DLL_EXPORT
#elif defined(RL_SHARED)
    // Use shared library
    #define DNAAPI DLL_IMPORT
#else
    // Build or use static library
    #define DNAAPI
#endif
