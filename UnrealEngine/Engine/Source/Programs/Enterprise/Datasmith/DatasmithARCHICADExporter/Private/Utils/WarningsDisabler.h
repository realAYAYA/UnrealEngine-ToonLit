// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// clang-format off

#if PLATFORM_WINDOWS

#define DISABLE_SDK_WARNINGS_START \
	__pragma(warning(push)) \
	__pragma(warning(disable: 4996)) \
	__pragma(warning(disable: 5038)) \
	__pragma(warning(disable: 5040)) \
	__pragma(warning(disable: 5208)) \
	__pragma(warning(disable: 26495)) \
	__pragma(warning(disable: 26451)) \
	__pragma(warning(disable: 26812)) \
	__pragma(warning(disable: 26450)) \
	__pragma(warning(disable: 4459)) \
	__pragma(warning(disable: 4005)) \
	__pragma(warning(disable: 5038)) \
	__pragma(warning(disable: 4275)) \
	__pragma(warning(disable: 4250))

#define DISABLE_SDK_WARNINGS_END \
	__pragma(warning(pop))

#else

#define DISABLE_SDK_WARNINGS_START_OLD \
    _Pragma( "clang diagnostic push" ) \
    _Pragma( "clang diagnostic ignored \"-Wdeprecated-declarations\"" ) \
    _Pragma( "clang diagnostic ignored \"-Wunused-parameter\"" ) \
    _Pragma( "clang diagnostic ignored \"-Wdocumentation\"" ) \
	_Pragma( "clang diagnostic ignored \"-Wshorten-64-to-32\"" ) \
    _Pragma( "clang diagnostic ignored \"-Wdefaulted-function-deleted\"" ) \
    _Pragma( "clang diagnostic ignored \"-Winconsistent-missing-override\"" ) \
    _Pragma( "clang diagnostic ignored \"-Wtautological-undefined-compare\"" ) \
	_Pragma( "clang diagnostic ignored \"-Wcomma\"" )

#if (__clang_major__ > 12) || (__clang_major__ == 12 && __clang_minor__ == 0 && __clang_patchlevel__ > 4)
	#define DISABLE_SDK_WARNINGS_START DISABLE_SDK_WARNINGS_START_OLD _Pragma( "clang diagnostic ignored \"-Wnon-c-typedef-for-linkage\"" )
#else
	#define DISABLE_SDK_WARNINGS_START DISABLE_SDK_WARNINGS_START_OLD
#endif

#define DISABLE_SDK_WARNINGS_END \
    _Pragma( "clang diagnostic pop" )

#endif

// clang-format on
