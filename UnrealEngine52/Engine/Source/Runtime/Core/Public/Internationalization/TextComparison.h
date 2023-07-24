// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

namespace ETextComparisonLevel
{
	enum Type
	{
		Default,	// Locale-specific Default
		Primary,	// Base
		Secondary,	// Accent
		Tertiary,	// Case
		Quaternary,	// Punctuation
		Quinary		// Identical
	};
}

/**
 * Utility for performing low-level text comparison.
 * The implementation can be found in LegacyText.cpp and ICUText.cpp.
 */
class CORE_API FTextComparison
{
public:
	static int32 CompareTo(const FString& A, const FString& B, const ETextComparisonLevel::Type ComparisonLevel = ETextComparisonLevel::Default);
	static int32 CompareToCaseIgnored(const FString& A, const FString& B);

	static bool EqualTo(const FString& A, const FString& B, const ETextComparisonLevel::Type ComparisonLevel = ETextComparisonLevel::Default);
	static bool EqualToCaseIgnored(const FString& A, const FString& B);
};
