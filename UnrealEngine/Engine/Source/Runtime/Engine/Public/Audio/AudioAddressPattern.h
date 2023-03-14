// Copyright Epic Games, Inc. All Rights Reserved.
// Pattern-Matching that fulfills the OSC 1.0 Protocol on address pattern matching
#pragma once

#include "CoreMinimal.h"

class ENGINE_API FAudioAddressPattern
{
	static int32 FindPatternTerminatorIndex(const FString& Pattern, int32 PatternIter, TCHAR Terminator);

	static const TArray<TCHAR>& GetInvalidChars();
	static const TArray<TCHAR>& GetPatternChars();
	
public:
	static bool BracePatternMatches(const FString& Pattern, int32 PatternStartIndex, int32 PatternEndIndex, const FString& Part, int32& PartIter);
	static bool BracketPatternMatches(const FString& Pattern, int32 PatternStartIndex, int32 PatternEndIndex, TCHAR MatchChar);
	static bool IsValidPatternPart(const FString& Part);
	static bool IsValidPattern(const TArray<FString>& InContainers, const FString& InMethod);
	static bool IsValidPath(const FString& Path, bool bInvalidateSeparator);
	static bool PartsMatch(const FString& Pattern, const FString& Part);
	static bool WildPatternMatches(const FString& Pattern, int32& PatternIter, const FString& Part, int32& PartIter);
};