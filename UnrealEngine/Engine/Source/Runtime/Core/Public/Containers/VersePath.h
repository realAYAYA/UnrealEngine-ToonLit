// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VersePath.h: A type which holds a VersePath
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "VersePathFwd.h"

class UE::Core::FVersePath
{
	friend bool operator==(const FVersePath& Lhs, const FVersePath& Rhs);
	friend bool operator!=(const FVersePath& Lhs, const FVersePath& Rhs);

	friend FArchive& operator<<(FArchive& Ar, FVersePath& VersePath);

public:
	FVersePath() = default;
	FVersePath(FVersePath&&) = default;
	FVersePath(const FVersePath&) = default;
	FVersePath& operator=(FVersePath&&) = default;
	FVersePath& operator=(const FVersePath&) = default;
	~FVersePath() = default;

	const TCHAR* operator*() const
	{
		return *PathString;
	}

	bool IsValid() const
	{
		return !PathString.IsEmpty();
	}

	explicit operator bool() const
	{
		return !PathString.IsEmpty();
	}

	FString ToString() const&
	{
		return PathString;
	}

	FString ToString() &&
	{
		return MoveTemp(PathString);
	}

	FStringView AsStringView() const
	{
		return PathString;
	}

	CORE_API static bool TryMake(FVersePath& OutPath, const FString& Path);
	CORE_API static bool TryMake(FVersePath& OutPath, FString&& Path);

	CORE_API static bool IsValidFullPath(const TCHAR* String);
	CORE_API static bool IsValidFullPath(const TCHAR* String, int32 Len);
	CORE_API static bool IsValidDomain(const TCHAR* String);
	CORE_API static bool IsValidDomain(const TCHAR* String, int32 Len);
	CORE_API static bool IsValidSubpath(const TCHAR* String);
	CORE_API static bool IsValidSubpath(const TCHAR* String, int32 Len);
	CORE_API static bool IsValidIdent(const TCHAR* String);
	CORE_API static bool IsValidIdent(const TCHAR* String, int32 Len);

private:
	FString PathString;
};

FORCEINLINE bool UE::Core::operator==(const FVersePath& Lhs, const FVersePath& Rhs)
{
	return Lhs.PathString.Equals(Rhs.PathString, ESearchCase::CaseSensitive);
}

FORCEINLINE bool UE::Core::operator!=(const FVersePath& Lhs, const FVersePath& Rhs)
{
	return !(Lhs == Rhs);
}

FORCEINLINE FArchive& UE::Core::operator<<(FArchive& Ar, FVersePath& VersePath)
{
	return Ar << VersePath.PathString;
}

FORCEINLINE uint32 GetTypeHash(const UE::Core::FVersePath& VersePath) // Must be outside namespace to not break Tuples. Note that it needs to match VersePathFwd.h so can't be hidden friend
{
	return FCrc::StrCrc32<TCHAR>(*VersePath);
}
