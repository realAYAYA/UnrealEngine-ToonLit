// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VersePath.h: Forward declarations of VersePath-related types
=============================================================================*/

#pragma once

#ifndef UE_USE_VERSE_PATHS
	#define UE_USE_VERSE_PATHS 0
#endif

#if UE_USE_VERSE_PATHS

class FArchive;

namespace UE::Core
{
	class FVersePath;

	bool operator==(const FVersePath& Lhs, const FVersePath& Rhs);
	bool operator!=(const FVersePath& Lhs, const FVersePath& Rhs);

	FArchive& operator<<(FArchive& Ar, FVersePath& VersePath);

	uint32 GetTypeHash(const FVersePath& VersePath);
}

#endif // #if UE_USE_VERSE_PATHS
