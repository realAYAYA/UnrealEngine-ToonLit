// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VersePath.h: Forward declarations of VersePath-related types
=============================================================================*/

#pragma once

#ifndef UE_USE_VERSE_PATHS
	#define UE_USE_VERSE_PATHS 0
#endif

#if UE_USE_VERSE_PATHS

#include "CoreTypes.h"

class FArchive;
class FString;

namespace UE::Core
{
	class FVersePath;

	bool operator==(const FVersePath& Lhs, const FVersePath& Rhs);
	bool operator!=(const FVersePath& Lhs, const FVersePath& Rhs);

	FArchive& operator<<(FArchive& Ar, FVersePath& VersePath);

	CORE_API FString MangleGuidToVerseIdent(const FString& Guid);
}

uint32 GetTypeHash(const UE::Core::FVersePath& VersePath); // Must be outside namespace to not break Tuples

#endif // #if UE_USE_VERSE_PATHS
