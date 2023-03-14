// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/AssertionMacros.h"

#if defined(CATCH_CONFIG_PREFIX_ALL) && !defined(CATCH_CONFIG_DISABLE)
    #define CATCH_REQUIRE_ENSURE( ... ) INTERNAL_CATCH_ENSURE( "CATCH_REQUIRE_ENSURE", Catch::ResultDisposition::Normal, __VA_ARGS__ )
    #define CATCH_REQUIRE_NOENSURE( ... ) INTERNAL_CATCH_NO_ENSURE( "CATCH_REQUIRE_NOENSURE", Catch::ResultDisposition::Normal, __VA_ARGS__ )
    #define CATCH_CHECK_ENSURE( ... ) INTERNAL_CATCH_ENSURE( "CATCH_CHECK_ENSURE", Catch::ResultDisposition::ContinueOnFailure, __VA_ARGS__ )
    #define CATCH_CHECK_NOENSURE( ... ) INTERNAL_CATCH_NO_ENSURE( "CATCH_CHECK_NOENSURE", Catch::ResultDisposition::ContinueOnFailure, __VA_ARGS__ )
#else
    #define REQUIRE_ENSURE( ... ) INTERNAL_CATCH_ENSURE( "REQUIRE_ENSURE", Catch::ResultDisposition::Normal, __VA_ARGS__ )
    #define REQUIRE_NOENSURE( ... ) INTERNAL_CATCH_NO_ENSURE( "REQUIRE_NOENSURE", Catch::ResultDisposition::Normal, __VA_ARGS__ )
    #define CHECK_ENSURE( ... ) INTERNAL_CATCH_ENSURE( "CHECK_ENSURE", Catch::ResultDisposition::ContinueOnFailure, __VA_ARGS__ )
    #define CHECK_NOENSURE( ... ) INTERNAL_CATCH_NO_ENSURE( "CHECK_NOENSURE", Catch::ResultDisposition::ContinueOnFailure, __VA_ARGS__ )
#endif

#define INTERNAL_CATCH_ENSURE( macroName, resultDisposition, ... ) \
 do { \
		SIZE_T NumEnsureFailuresBefore = FDebug::GetNumEnsureFailures(); \
        Catch::AssertionHandler catchAssertionHandler( macroName##_catch_sr, CATCH_INTERNAL_LINEINFO, CATCH_INTERNAL_STRINGIFY(__VA_ARGS__), resultDisposition ); \
        INTERNAL_CATCH_TRY { \
            static_cast<void>(__VA_ARGS__); \
            bool bEncounteredEnsure = (FDebug::GetNumEnsureFailures() - NumEnsureFailuresBefore) > 0; \
			catchAssertionHandler.handleExpr( Catch::Decomposer() <= bEncounteredEnsure ); \
        } INTERNAL_CATCH_CATCH( catchAssertionHandler ) \
        INTERNAL_CATCH_REACT( catchAssertionHandler ) \
    } while( false )

#define INTERNAL_CATCH_NO_ENSURE( macroName, resultDisposition, ... ) \
 do { \
		SIZE_T NumEnsureFailuresBefore = FDebug::GetNumEnsureFailures(); \
        Catch::AssertionHandler catchAssertionHandler( macroName##_catch_sr, CATCH_INTERNAL_LINEINFO, CATCH_INTERNAL_STRINGIFY(__VA_ARGS__), resultDisposition ); \
        INTERNAL_CATCH_TRY { \
            static_cast<void>(__VA_ARGS__); \
            bool bEncounteredEnsure = (FDebug::GetNumEnsureFailures() - NumEnsureFailuresBefore) > 0; \
			catchAssertionHandler.handleExpr( Catch::Decomposer() <= !(bEncounteredEnsure) ); \
        } INTERNAL_CATCH_CATCH( catchAssertionHandler ) \
        INTERNAL_CATCH_REACT( catchAssertionHandler ) \
    } while( false )
