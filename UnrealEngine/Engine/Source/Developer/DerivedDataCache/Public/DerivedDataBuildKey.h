// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"
#include "DerivedDataValueId.h"
#include "IO/IoHash.h"
#include "Templates/TypeHash.h"

namespace UE::DerivedData
{

/** A key that uniquely identifies a build definition. */
struct FBuildKey
{
	FIoHash Hash;

	/** A key with a zero hash. */
	static const FBuildKey Empty;
};

inline const FBuildKey FBuildKey::Empty;

/** A key that uniquely identifies a build action. */
struct FBuildActionKey
{
	FIoHash Hash;

	/** A key with a zero hash. */
	static const FBuildActionKey Empty;
};

inline const FBuildActionKey FBuildActionKey::Empty;

/** A key that uniquely identifies a value within a build output. */
struct FBuildValueKey
{
	FBuildKey BuildKey;
	FValueId Id;

	/** A value key with an empty build key and a null value identifier. */
	static const FBuildValueKey Empty;
};

inline const FBuildValueKey FBuildValueKey::Empty;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool operator==(const FBuildKey& A, const FBuildKey& B)
{
	return A.Hash == B.Hash;
}

inline bool operator!=(const FBuildKey& A, const FBuildKey& B)
{
	return A.Hash != B.Hash;
}

inline bool operator<(const FBuildKey& A, const FBuildKey& B)
{
	return A.Hash < B.Hash;
}

inline uint32 GetTypeHash(const FBuildKey& Key)
{
	return GetTypeHash(Key.Hash);
}

template <typename CharType>
inline TStringBuilderBase<CharType>& operator<<(TStringBuilderBase<CharType>& Builder, const FBuildKey& Key)
{
	return Builder << ANSITEXTVIEW("Build/") << Key.Hash;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool operator==(const FBuildActionKey& A, const FBuildActionKey& B)
{
	return A.Hash == B.Hash;
}

inline bool operator!=(const FBuildActionKey& A, const FBuildActionKey& B)
{
	return A.Hash != B.Hash;
}

inline bool operator<(const FBuildActionKey& A, const FBuildActionKey& B)
{
	return A.Hash < B.Hash;
}

inline uint32 GetTypeHash(const FBuildActionKey& Key)
{
	return GetTypeHash(Key.Hash);
}

template <typename CharType>
inline TStringBuilderBase<CharType>& operator<<(TStringBuilderBase<CharType>& Builder, const FBuildActionKey& Key)
{
	return Builder << ANSITEXTVIEW("BuildAction/") << Key.Hash;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool operator==(const FBuildValueKey& A, const FBuildValueKey& B)
{
	return A.BuildKey == B.BuildKey && A.Id == B.Id;
}

inline bool operator!=(const FBuildValueKey& A, const FBuildValueKey& B)
{
	return A.BuildKey != B.BuildKey || A.Id != B.Id;
}

inline bool operator<(const FBuildValueKey& A, const FBuildValueKey& B)
{
	const FBuildKey& KeyA = A.BuildKey;
	const FBuildKey& KeyB = B.BuildKey;
	return KeyA == KeyB ? A.Id < B.Id : KeyA < KeyB;
}

inline uint32 GetTypeHash(const FBuildValueKey& Key)
{
	return HashCombine(GetTypeHash(Key.BuildKey), GetTypeHash(Key.Id));
}

template <typename CharType>
inline TStringBuilderBase<CharType>& operator<<(TStringBuilderBase<CharType>& Builder, const FBuildValueKey& Key)
{
	return Builder << Key.BuildKey << CharType('/') << Key.Id;
}

} // UE::DerivedData
