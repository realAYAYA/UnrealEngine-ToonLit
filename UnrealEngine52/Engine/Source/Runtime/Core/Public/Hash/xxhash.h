// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/StringFwd.h"
#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Memory/MemoryFwd.h"
#include "Misc/ByteSwap.h"
#include "Serialization/Archive.h"
#include "String/BytesToHex.h"

class FCompositeBuffer;
template <typename CharType> class TStringBuilderBase;

/** A 64-bit hash from XXH3. */
struct FXxHash64
{
	[[nodiscard]] CORE_API static FXxHash64 HashBuffer(FMemoryView View);
	[[nodiscard]] CORE_API static FXxHash64 HashBuffer(const void* Data, uint64 Size);
	[[nodiscard]] CORE_API static FXxHash64 HashBuffer(const FCompositeBuffer& Buffer);

	/** Load the hash from its canonical (big-endian) representation. */
	static inline FXxHash64 FromByteArray(uint8 (&Bytes)[sizeof(uint64)])
	{
		uint64 HashBigEndian;
		FMemory::Memcpy(&HashBigEndian, Bytes, sizeof(uint64));
		return {NETWORK_ORDER64(HashBigEndian)};
	}

	/** Store the hash to its canonical (big-endian) representation. */
	inline void ToByteArray(uint8 (&Bytes)[sizeof(uint64)]) const
	{
		const uint64 HashBigEndian = NETWORK_ORDER64(Hash);
		FMemory::Memcpy(Bytes, &HashBigEndian, sizeof(uint64));
	}

	inline bool operator==(const FXxHash64& B) const
	{
		return Hash == B.Hash;
	}

	inline bool operator!=(const FXxHash64& B) const
	{
		return Hash != B.Hash;
	}

	inline bool operator<(const FXxHash64& B) const
	{
		return Hash < B.Hash;
	}

	friend inline uint32 GetTypeHash(const FXxHash64& InHash)
	{
		return uint32(InHash.Hash);
	}

	friend inline FArchive& operator<<(FArchive& Ar, FXxHash64& InHash)
	{
		return Ar << InHash.Hash;
	}

	template <typename CharType>
	friend inline TStringBuilderBase<CharType>& operator<<(TStringBuilderBase<CharType>& Builder, const FXxHash64& InHash)
	{
		uint8 Bytes[8];
		InHash.ToByteArray(Bytes);
		UE::String::BytesToHexLower(MakeArrayView(Bytes), Builder);
		return Builder;
	}


	/**
	 * The hash in its native representation.
	 *
	 * Use the canonical representation from ToByteArray to serialize or display the hash.
	 */
	uint64 Hash{};
};

/** A 128-bit hash from XXH3. */
struct FXxHash128
{
public:
	[[nodiscard]] CORE_API static FXxHash128 HashBuffer(FMemoryView View);
	[[nodiscard]] CORE_API static FXxHash128 HashBuffer(const void* Data, uint64 Size);
	[[nodiscard]] CORE_API static FXxHash128 HashBuffer(const FCompositeBuffer& Buffer);

	/** Load the hash from its canonical (big-endian) representation. */
	static inline FXxHash128 FromByteArray(uint8 (&Bytes)[sizeof(uint64[2])])
	{
		uint64 HashBigEndian[2];
		FMemory::Memcpy(&HashBigEndian, Bytes, sizeof(uint64[2]));
		return {NETWORK_ORDER64(HashBigEndian[1]), NETWORK_ORDER64(HashBigEndian[0])};
	}

	/** Store the hash to its canonical (big-endian) representation. */
	inline void ToByteArray(uint8 (&Bytes)[sizeof(uint64[2])]) const
	{
		const uint64 HashBigEndian[2]{NETWORK_ORDER64(HashHigh), NETWORK_ORDER64(HashLow)};
		FMemory::Memcpy(Bytes, &HashBigEndian, sizeof(uint64[2]));
	}

	/**
	 * The low 64 bits of the hash in its native representation.
	 *
	 * Use the canonical representation from ToByteArray to serialize or display the hash.
	 */
	uint64 HashLow{};
	/**
	 * The high 64 bits of the hash in its native representation.
	 *
	 * Use the canonical representation from ToByteArray to serialize or display the hash.
	 */
	uint64 HashHigh{};

	inline bool operator==(const FXxHash128& B) const
	{
		return HashLow == B.HashLow && HashHigh == B.HashHigh;
	}

	inline bool operator!=(const FXxHash128& B) const
	{
		return HashLow != B.HashLow || HashHigh != B.HashHigh;
	}

	inline bool operator<(const FXxHash128& B) const
	{
		return HashHigh != B.HashHigh ? HashHigh < B.HashHigh : HashLow < B.HashLow;
	}

	friend inline uint32 GetTypeHash(const FXxHash128& Hash)
	{
		return uint32(Hash.HashLow);
	}

	friend inline FArchive& operator<<(FArchive& Ar, FXxHash128& Hash)
	{
		return Ar << Hash.HashLow << Hash.HashHigh;
	}

	template <typename CharType>
	friend inline TStringBuilderBase<CharType>& operator<<(TStringBuilderBase<CharType>& Builder, const FXxHash128& Hash)
	{
		uint8 Bytes[16];
		Hash.ToByteArray(Bytes);
		UE::String::BytesToHexLower(MakeArrayView(Bytes), Builder);
		return Builder;
	}
};

/** Calculates a 64-bit hash with XXH3. */
class FXxHash64Builder
{
public:
	inline FXxHash64Builder() { Reset(); }

	FXxHash64Builder(const FXxHash64Builder&) = delete;
	FXxHash64Builder& operator=(const FXxHash64Builder&) = delete;

	CORE_API void Reset();

	CORE_API void Update(FMemoryView View);
	CORE_API void Update(const void* Data, uint64 Size);
	CORE_API void Update(const FCompositeBuffer& Buffer);

	[[nodiscard]] CORE_API FXxHash64 Finalize() const;

private:
	alignas(64) char StateBytes[576];
};

/** Calculates a 128-bit hash with XXH3. */
class FXxHash128Builder
{
public:
	inline FXxHash128Builder() { Reset(); }

	FXxHash128Builder(const FXxHash128Builder&) = delete;
	FXxHash128Builder& operator=(const FXxHash128Builder&) = delete;

	CORE_API void Reset();

	CORE_API void Update(FMemoryView View);
	CORE_API void Update(const void* Data, uint64 Size);
	CORE_API void Update(const FCompositeBuffer& Buffer);

	[[nodiscard]] CORE_API FXxHash128 Finalize() const;

private:
	alignas(64) char StateBytes[576];
};
