// Copyright Epic Games, Inc. All Rights Reserved.

#include "StringUtils.h"
#include "UnrealHeaderTool.h"
#include "Containers/UnrealString.h"
#include "Hash/CityHash.h"

FString GetClassNameWithPrefixRemoved(const FString& InClassName)
{
	FString Result;
	const FString ClassPrefix = GetClassPrefix( InClassName );
	if( !ClassPrefix.IsEmpty() )
	{	
		Result = InClassName.Right(InClassName.Len() - ClassPrefix.Len());
	}
	return Result;
}

FString GetClassNameWithoutPrefix(FString InClassNameOrFilename)
{
	FString ClassNameWithoutPrefix = MoveTemp(InClassNameOrFilename);

	// Check for header names (they don't come with a full path so we only search for the first dot)
	int32 DotIndex;
	if (ClassNameWithoutPrefix.FindChar(TEXT('.'), DotIndex))
	{
		ClassNameWithoutPrefix.MidInline(0, DotIndex, false);
	}
	else
	{
		const FString ClassPrefix = GetClassPrefix(ClassNameWithoutPrefix);
		ClassNameWithoutPrefix.RightInline(ClassNameWithoutPrefix.Len() - ClassPrefix.Len(), false);
	}
	return ClassNameWithoutPrefix;
}

FString GetClassPrefix( const FString& InClassName )
{
	bool bIsLabledDeprecated;
	return GetClassPrefix(InClassName, /*out*/ bIsLabledDeprecated);
}

FString GetClassPrefix(const FString& InClassName, bool& bIsLabeledDeprecated )
{
	FString ClassPrefix = InClassName.Left(1);

	bIsLabeledDeprecated = false;

	if (!ClassPrefix.IsEmpty())
	{
		const TCHAR ClassPrefixChar = ClassPrefix[0];
		switch (ClassPrefixChar)
		{
		case TEXT('I'):
		case TEXT('A'):
		case TEXT('U'):
			// If it is a class prefix, check for deprecated class prefix also
			if (InClassName.Len() > 12 && FCString::Strncmp(&(InClassName[1]), TEXT("DEPRECATED_"), 11) == 0)
			{
				bIsLabeledDeprecated = true;
				ClassPrefix = InClassName.Left(12);
			}
			break;

		case TEXT('F'):
		case TEXT('T'):
			// Struct prefixes are also fine.
			break;

		default:
			// If it's not a class or struct prefix, it's invalid
			ClassPrefix.Reset();
			break;
		}
	}
	return ClassPrefix;
}

// Returns zero only for '\r' and '\0' 
FORCEINLINE uint64 FindCrOrNulHelper(TCHAR C)
{
	if (C == '\r')
	{
		return 0;
	}
	return C;

	// NOTE: This is the previous implementation. It might have been faster
	// once but current compilers do a much better job and compile into
	// conditional moves which are faster than the variable-count bit shift below
#if 0
	uint64 LoPassMask = ~((uint64(1) << '\0') | (uint64(1) << '\r'));
	uint64 LoBit = uint64(1) << (C & 63);
	uint64 LoPass = LoBit & LoPassMask;
	uint64 HiPass = C & ~63;

	return LoPass | HiPass;
#endif
}

// Finds next CR or null term with a single branch 
// to help treat CR-LF and LF line endings identically
FORCEINLINE const TCHAR* FindCrOrNul(const TCHAR* Str)
{
	while (FindCrOrNulHelper(*Str))
	{
		++Str;
	}

	return Str;
}

FORCEINLINE uint64 GenerateTextHash64(const TCHAR* Str)
{
	uint64 Hash = 0;

	while (true)
	{
		const TCHAR* End = FindCrOrNul(Str);
		
		if (int32 Len = UE_PTRDIFF_TO_INT32(End - Str))
		{
			Hash = CityHash64WithSeed(reinterpret_cast<const char*>(Str), Len * sizeof(TCHAR), Hash);
		}

		if (*End == '\0')
		{
			return Hash;
		}

		Str = End + 1;
	}
}

uint32 GenerateTextHash(const TCHAR* Data)
{
	uint64 Hash = GenerateTextHash64(Data);
	return static_cast<uint32>(Hash) + static_cast<uint32>(Hash >> 32);
}