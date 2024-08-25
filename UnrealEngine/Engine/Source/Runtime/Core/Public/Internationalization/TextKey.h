// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/TypeHash.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Serialization/StructuredArchive.h"

#ifndef UE_TEXTKEY_STORE_EMBEDDED_HASH
	#define UE_TEXTKEY_STORE_EMBEDDED_HASH (1)
#endif

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

	/** Utility to produce a hash for a string (as used by FTextKey) */
	FORCEINLINE uint32 HashString(FStringView InStr)
	{
		return HashString(InStr.GetData(), InStr.Len());
	}
	FORCEINLINE uint32 HashString(FStringView InStr, const uint32 InBaseHash)
	{
		return HashString(InStr.GetData(), InStr.Len(), InBaseHash);
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
	CORE_API FTextKey(FStringView InStr);
	
	FTextKey(const TCHAR* InStr)
		: FTextKey(FStringView(InStr))
	{
	}

	FTextKey(const FString& InStr)
		: FTextKey(FStringView(InStr))
	{
	}

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
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		return A.StrHash;
#else
		return TextKeyUtil::HashString(A.StrPtr);
#endif
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
	/** Pointer to the null-terminated string buffer we reference */
	const TCHAR* StrPtr;

#if UE_TEXTKEY_STORE_EMBEDDED_HASH
	/** Hash of this text key */
	uint32 StrHash;
#endif

	FTextKey(const TCHAR* Str, uint32 Hash)
		: StrPtr(Str)
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		, StrHash(Hash)
#endif
	{
	}

	friend class FTextId;
	friend class FTextKeyState;
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
	{
		SetNamespace(InNamespace);
		SetKey(InKey);
	}

	/** Get the namespace component of this text identity */
	FORCEINLINE FTextKey GetNamespace() const
	{
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		return FTextKey(NamespaceStr, NamespaceHash);
#else
		return FTextKey(NamespaceStr, 0);
#endif
	}

	/** Get the key component of this text identity */
	FORCEINLINE FTextKey GetKey() const
	{
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		return FTextKey(KeyStr, KeyHash);
#else
		return FTextKey(KeyStr, 0);
#endif
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
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		return HashCombine(A.NamespaceHash, A.KeyHash);
#else
		return HashCombine(TextKeyUtil::HashString(A.NamespaceStr), TextKeyUtil::HashString(A.KeyStr));
#endif
	}

	/** Serialize this text identity as if it were FStrings */
	void SerializeAsString(FArchive& Ar)
	{
		FTextKey Namespace = GetNamespace();
		Namespace.SerializeAsString(Ar);
		SetNamespace(Namespace);

		FTextKey Key = GetKey();
		Key.SerializeAsString(Ar);
		SetKey(Key);
	}

	/** Serialize this text identity including its hash values (this method is sensitive to hashing algorithm changes, so only use it for generated files that can be rebuilt from another source) */
	void SerializeWithHash(FArchive& Ar)
	{
		FTextKey Namespace = GetNamespace();
		Namespace.SerializeWithHash(Ar);
		SetNamespace(Namespace);

		FTextKey Key = GetKey();
		Key.SerializeWithHash(Ar);
		SetKey(Key);
	}

	/** Serialize this text identity including its hash values, discarding the hash on load (to upgrade from an older hashing algorithm) */
	void SerializeDiscardHash(FArchive& Ar)
	{
		FTextKey Namespace = GetNamespace();
		Namespace.SerializeDiscardHash(Ar);
		SetNamespace(Namespace);

		FTextKey Key = GetKey();
		Key.SerializeDiscardHash(Ar);
		SetKey(Key);
	}

	/** Serialize this text identity as if it were FStrings */
	void SerializeAsString(FStructuredArchiveSlot Slot)
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();

		FTextKey Namespace = GetNamespace();
		Namespace.SerializeAsString(Record.EnterField(TEXT("Namespace")));
		SetNamespace(Namespace);

		FTextKey Key = GetKey();
		Key.SerializeAsString(Record.EnterField(TEXT("Key")));
		SetKey(Key);
	}

	/** Serialize this text identity including its hash values (this method is sensitive to hashing algorithm changes, so only use it for generated files that can be rebuilt from another source) */
	void SerializeWithHash(FStructuredArchiveSlot Slot)
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();

		FTextKey Namespace = GetNamespace();
		Namespace.SerializeWithHash(Record.EnterField(TEXT("Namespace")));
		SetNamespace(Namespace);

		FTextKey Key = GetKey();
		Key.SerializeWithHash(Record.EnterField(TEXT("Key")));
		SetKey(Key);
	}

	/** Serialize this text identity including its hash values, discarding the hash on load (to upgrade from an older hashing algorithm) */
	void SerializeDiscardHash(FStructuredArchiveSlot Slot)
	{
		FStructuredArchiveRecord Record = Slot.EnterRecord();

		FTextKey Namespace = GetNamespace();
		Namespace.SerializeDiscardHash(Record.EnterField(TEXT("Namespace")));
		SetNamespace(Namespace);

		FTextKey Key = GetKey();
		Key.SerializeDiscardHash(Record.EnterField(TEXT("Key")));
		SetKey(Key);
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
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		NamespaceHash = KeyHash = 0;
#endif
	}

private:
	void SetNamespace(const FTextKey& InNamespace)
	{
		NamespaceStr = InNamespace.StrPtr;
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		NamespaceHash = InNamespace.StrHash;
#endif
	}

	void SetKey(const FTextKey& InKey)
	{
		KeyStr = InKey.StrPtr;
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
		KeyHash = InKey.StrHash;
#endif
	}

	const TCHAR* NamespaceStr;
	const TCHAR* KeyStr;
#if UE_TEXTKEY_STORE_EMBEDDED_HASH
	uint32 NamespaceHash;
	uint32 KeyHash;
#endif
};
