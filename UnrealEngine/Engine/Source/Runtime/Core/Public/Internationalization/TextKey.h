// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/TypeHash.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "Serialization/StructuredArchive.h"

namespace TextKeyUtil
{
	/** Utility to produce a hash for a UTF-16 string (as used by FTextKey) */
	CORE_API uint32 HashString(const FTCHARToUTF16& InStr);
	FORCEINLINE uint32 HashString(const FTCHARToUTF16& InStr, const uint32 InBaseHash)
	{
		return HashCombine(HashString(InStr), InBaseHash);
	}

	/** Utility to produce a hash for a string (as used by FTextKey) */
	FORCEINLINE uint32 HashString(const TCHAR* InStr)
	{
		FTCHARToUTF16 UTF16String(InStr);
		return HashString(UTF16String);
	}
	FORCEINLINE uint32 HashString(const TCHAR* InStr, const uint32 InBaseHash)
	{
		FTCHARToUTF16 UTF16String(InStr);
		return HashString(UTF16String, InBaseHash);
	}

	/** Utility to produce a hash for a string (as used by FTextKey) */
	FORCEINLINE uint32 HashString(const TCHAR* InStr, const int32 InStrLen)
	{
		FTCHARToUTF16 UTF16String(InStr, InStrLen);
		return HashString(UTF16String);
	}
	FORCEINLINE uint32 HashString(const TCHAR* InStr, const int32 InStrLen, const uint32 InBaseHash)
	{
		FTCHARToUTF16 UTF16String(InStr, InStrLen);
		return HashString(UTF16String, InBaseHash);
	}

	/** Utility to produce a hash for a string (as used by FTextKey) */
	FORCEINLINE uint32 HashString(const FString& InStr)
	{
		return HashString(*InStr, InStr.Len());
	}
	FORCEINLINE uint32 HashString(const FString& InStr, const uint32 InBaseHash)
	{
		return HashString(*InStr, InStr.Len(), InBaseHash);
	}
}

/**
 * Optimized representation of a case-sensitive string, as used by localization keys.
 * This references an entry within a internal table to avoid memory duplication, as well as offering optimized comparison and hashing performance.
 */
class FTextKey
{
public:
	CORE_API FTextKey();
	CORE_API FTextKey(const TCHAR* InStr);
	CORE_API FTextKey(const FString& InStr);
	CORE_API FTextKey(FString&& InStr);

	/** Get the underlying chars buffer this text key represents */
	FORCEINLINE const TCHAR* GetChars() const
	{
		return StrPtr;
	}

	/** Compare for equality */
	friend FORCEINLINE bool operator==(const FTextKey& A, const FTextKey& B)
	{
		return A.StrPtr == B.StrPtr;
	}

	/** Compare for inequality */
	friend FORCEINLINE bool operator!=(const FTextKey& A, const FTextKey& B)
	{
		return A.StrPtr != B.StrPtr;
	}

	/** Get the hash of this text key */
	friend FORCEINLINE uint32 GetTypeHash(const FTextKey& A)
	{
		return A.StrHash;
	}

	/** Serialize this text key as if it were an FString */
	CORE_API void SerializeAsString(FArchive& Ar);

	/** Serialize this text key including its hash value (this method is sensitive to hashing algorithm changes, so only use it for generated files that can be rebuilt from another source) */
	CORE_API void SerializeWithHash(FArchive& Ar);

	/** Serialize this text key including its hash value, discarding the hash on load (to upgrade from an older hashing algorithm) */
	CORE_API void SerializeDiscardHash(FArchive& Ar);

	/** Serialize this text key as if it were an FString */
	CORE_API void SerializeAsString(FStructuredArchiveSlot Slot);

	/** Serialize this text key including its hash value (this method is sensitive to hashing algorithm changes, so only use it for generated files that can be rebuilt from another source) */
	CORE_API void SerializeWithHash(FStructuredArchiveSlot Slot);

	/** Serialize this text key including its hash value, discarding the hash on load (to upgrade from an older hashing algorithm) */
	CORE_API void SerializeDiscardHash(FStructuredArchiveSlot Slot);

	/** Is this text key empty? */
	FORCEINLINE bool IsEmpty() const
	{
		return *StrPtr == 0;
	}

	/** Reset this text key to be empty */
	CORE_API void Reset();

	/** Compact any slack within the internal table */
	static CORE_API void CompactDataStructures();

	/** Do not use any FTextKey or FTextId after calling this */
	static CORE_API void TearDown();

private:
	/** Pointer to the string buffer we reference from the internal table */
	const TCHAR* StrPtr;

	/** Hash of this text key */
	uint32 StrHash;

	FTextKey(const TCHAR* Str, uint32 Hash) : StrPtr(Str), StrHash(Hash) {}

	friend class FTextId;
};

/**
 * Optimized representation of a text identity (a namespace and key pair).
 */
class FTextId
{
public:
	FTextId()
	{
		Reset();
	}

	FTextId(const FTextKey& InNamespace, const FTextKey& InKey)
		: NamespaceStr(InNamespace.StrPtr)
		, KeyStr(InKey.StrPtr)
		, NamespaceHash(InNamespace.StrHash)
		, KeyHash(InKey.StrHash)
	{
	}

	/** Get the namespace component of this text identity */
	FORCEINLINE FTextKey GetNamespace() const
	{
		return FTextKey(NamespaceStr, NamespaceHash);
	}

	/** Get the key component of this text identity */
	FORCEINLINE FTextKey GetKey() const
	{
		return FTextKey(KeyStr, KeyHash);
	}

	/** Compare for equality */
	friend FORCEINLINE bool operator==(const FTextId& A, const FTextId& B)
	{
		return A.NamespaceStr == B.NamespaceStr && A.KeyStr == B.KeyStr;
	}

	/** Compare for inequality */
	friend FORCEINLINE bool operator!=(const FTextId& A, const FTextId& B)
	{
		return A.NamespaceStr != B.NamespaceStr || A.KeyStr != B.KeyStr;
	}

	/** Get the hash of this text identity */
	friend FORCEINLINE uint32 GetTypeHash(const FTextId& A)
	{
		return HashCombine(A.NamespaceHash, A.KeyHash);
	}

	/** Serialize this text identity as if it were FStrings */
	void SerializeAsString(FArchive& Ar)
	{
		FTextKey Namespace = FTextKey(NamespaceStr, NamespaceHash);
		Namespace.SerializeAsString(Ar);
		NamespaceStr = Namespace.StrPtr;
		NamespaceHash = Namespace.StrHash;

		FTextKey Key = FTextKey(KeyStr, KeyHash);
		Key.SerializeAsString(Ar);
		KeyStr = Key.StrPtr;
		KeyHash = Key.StrHash;
	}

	/** Serialize this text identity including its hash values (this method is sensitive to hashing algorithm changes, so only use it for generated files that can be rebuilt from another source) */
	void SerializeWithHash(FArchive& Ar)
	{
		FTextKey Namespace = FTextKey(NamespaceStr, NamespaceHash);
		Namespace.SerializeWithHash(Ar);
		NamespaceStr = Namespace.StrPtr;
		NamespaceHash = Namespace.StrHash;

		FTextKey Key = FTextKey(KeyStr, KeyHash);
		Key.SerializeWithHash(Ar);
		KeyStr = Key.StrPtr;
		KeyHash = Key.StrHash;
	}

	/** Serialize this text identity including its hash values, discarding the hash on load (to upgrade from an older hashing algorithm) */
	void SerializeDiscardHash(FArchive& Ar)
	{
		FTextKey Namespace = FTextKey(NamespaceStr, NamespaceHash);
		Namespace.SerializeDiscardHash(Ar);
		NamespaceStr = Namespace.StrPtr;
		NamespaceHash = Namespace.StrHash;

		FTextKey Key = FTextKey(KeyStr, KeyHash);
		Key.SerializeDiscardHash(Ar);
		KeyStr = Key.StrPtr;
		KeyHash = Key.StrHash;
	}

	/** Serialize this text identity as if it were FStrings */
	void SerializeAsString(FStructuredArchiveSlot Slot)
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();
		FTextKey Namespace = FTextKey(NamespaceStr, NamespaceHash);
		Namespace.SerializeAsString(Record.EnterField(TEXT("Namespace")));
		NamespaceStr = Namespace.StrPtr;
		NamespaceHash = Namespace.StrHash;

		FTextKey Key = FTextKey(KeyStr, KeyHash);
		Key.SerializeAsString(Record.EnterField(TEXT("Key")));
		KeyStr = Key.StrPtr;
		KeyHash = Key.StrHash;
	}

	/** Serialize this text identity including its hash values (this method is sensitive to hashing algorithm changes, so only use it for generated files that can be rebuilt from another source) */
	void SerializeWithHash(FStructuredArchiveSlot Slot)
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();
		FTextKey Namespace = FTextKey(NamespaceStr, NamespaceHash);
		Namespace.SerializeWithHash(Record.EnterField(TEXT("Namespace")));
		NamespaceStr = Namespace.StrPtr;
		NamespaceHash = Namespace.StrHash;

		FTextKey Key = FTextKey(KeyStr, KeyHash);
		Key.SerializeWithHash(Record.EnterField(TEXT("Key")));
		KeyStr = Key.StrPtr;
		KeyHash = Key.StrHash;
	}

	/** Serialize this text identity including its hash values, discarding the hash on load (to upgrade from an older hashing algorithm) */
	void SerializeDiscardHash(FStructuredArchiveSlot Slot)
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();
		FTextKey Namespace = FTextKey(NamespaceStr, NamespaceHash);
		Namespace.SerializeDiscardHash(Record.EnterField(TEXT("Namespace")));
		NamespaceStr = Namespace.StrPtr;
		NamespaceHash = Namespace.StrHash;

		FTextKey Key = FTextKey(KeyStr, KeyHash);
		Key.SerializeDiscardHash(Record.EnterField(TEXT("Key")));
		KeyStr = Key.StrPtr;
		KeyHash = Key.StrHash;
	}

	/** Is this text identity empty? */
	FORCEINLINE bool IsEmpty() const
	{
		return *NamespaceStr == 0 && *KeyStr == 0;
	}

	/** Reset this text identity to be empty */
	FORCEINLINE void Reset()
	{
		NamespaceStr = KeyStr = TEXT("");
		NamespaceHash = KeyHash = 0;
	}

private:
	const TCHAR* NamespaceStr;
	const TCHAR* KeyStr;
	uint32 NamespaceHash;
	uint32 KeyHash;
};
