// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compression/CompressedBuffer.h"
#include "CoreTypes.h"
#include "DerivedDataValueId.h"
#include "IO/IoHash.h"
#include "Memory/MemoryFwd.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"

#define UE_API DERIVEDDATACACHE_API

namespace UE::DerivedData
{

/**
 * A value is a compressed buffer identified by its raw hash and raw size.
 *
 * A value without data may be used as a reference to the value.
 */
class FValue
{
public:
	/**
	 * Compress the buffer to a value using default derived data compression parameters.
	 *
	 * @param RawData     The raw data to be compressed.
	 * @param BlockSize   The power-of-two block size to encode raw data in. 0 is default.
	 */
	UE_API static FValue Compress(const FCompositeBuffer& RawData, uint64 BlockSize = 0);
	UE_API static FValue Compress(const FSharedBuffer& RawData, uint64 BlockSize = 0);

	/** Construct a null value, which is a hash and size of zero with no data. */
	FValue() = default;

	/** Construct a value reference from the hash and size of its data. */
	inline FValue(const FIoHash& RawHash, uint64 RawSize);

	/** Construct a value from a compressed buffer, which is cloned if not owned. */
	inline explicit FValue(const FCompressedBuffer& Data);
	inline explicit FValue(FCompressedBuffer&& Data);

	/** Returns the hash of the raw buffer (uncompressed) for the value. */
	inline const FIoHash& GetRawHash() const { return RawHash; }

	/** Returns the size of the raw buffer (uncompressed) for the value. */
	inline uint64 GetRawSize() const { return RawSize; }

	/** Returns the compressed buffer for the value. May be null. */
	inline const FCompressedBuffer& GetData() const { return Data; }

	/** Whether the compressed buffer for the value is available. */
	inline bool HasData() const { return !!Data; }

	/** Create a copy of the value with the data removed. */
	[[nodiscard]] inline FValue RemoveData() const { return FValue(RawHash, RawSize); }

	/** Reset this to null. */
	inline void Reset() { *this = FValue(); }

	/** A null value. */
	static const FValue Null;

private:
	FIoHash RawHash;
	uint64 RawSize = 0;
	FCompressedBuffer Data;
};

/**
 * A value with an ID. Null if both ID and value are null.
 *
 * May have a null value with a non-null ID. A non-null value must have a non-null ID.
 */
class FValueWithId : public FValue
{
public:
	/** Construct a null value. */
	FValueWithId() = default;

	/** Construct a value with a non-null ID and forward the other args to FValue(). */
	template <typename... ArgTypes>
	inline explicit FValueWithId(const FValueId& Id, ArgTypes&&... Args);

	/** Returns the ID for the value. */
	inline const FValueId& GetId() const { return Id; }

	/** Create a copy of the value with the data removed. */
	[[nodiscard]] inline FValueWithId RemoveData() const { return FValueWithId(Id, FValue::RemoveData()); }

	/** Whether this is null. */
	inline bool IsNull() const { return Id.IsNull(); }
	/** Whether this is not null. */
	inline bool IsValid() const { return !IsNull(); }
	/** Whether this is not null. */
	inline explicit operator bool() const { return IsValid(); }

	/** Reset this to null. */
	inline void Reset() { FValue::Reset(); Id.Reset(); }

	/** A null value with a null ID. */
	static const FValueWithId Null;

private:
	FValueId Id;
};

inline const FValue FValue::Null;
inline const FValueWithId FValueWithId::Null;

inline FValue::FValue(const FIoHash& InRawHash, const uint64 InRawSize)
	: RawHash(InRawHash)
	, RawSize(InRawSize)
{
}

inline FValue::FValue(const FCompressedBuffer& InData)
	: RawHash(InData.GetRawHash())
	, RawSize(InData.GetRawSize())
	, Data(InData.MakeOwned())
{
}

inline FValue::FValue(FCompressedBuffer&& InData)
	: RawHash(InData.GetRawHash())
	, RawSize(InData.GetRawSize())
	, Data(MoveTemp(InData).MakeOwned())
{
}

template <typename... ArgTypes>
inline FValueWithId::FValueWithId(const FValueId& InId, ArgTypes&&... InArgs)
	: FValue(Forward<ArgTypes>(InArgs)...)
	, Id(InId)
{
	checkf(Id.IsValid(), TEXT("A valid ID is required to construct a value."));
}

/** Compare values by the hash and size of their raw buffer. */
inline bool operator==(const FValue& A, const FValue& B)
{
	return A.GetRawHash() == B.GetRawHash() && A.GetRawSize() == B.GetRawSize();
}

/** Compare values by their ID and the hash and size of their raw buffer. */
inline bool operator==(const FValueWithId& A, const FValueWithId& B)
{
	const FValue& ValueA = A;
	const FValue& ValueB = B;
	return A.GetId() == B.GetId() && ValueA == ValueB;
}

/** Compare values by the hash and size of their raw buffer. */
inline bool operator<(const FValue& A, const FValue& B)
{
	const FIoHash& HashA = A.GetRawHash();
	const FIoHash& HashB = B.GetRawHash();
	return !(HashA == HashB) ? HashA < HashB : A.GetRawSize() < B.GetRawSize();
}

/** Compare values by their ID and the hash and size of their raw buffer. */
inline bool operator<(const FValueWithId& A, const FValueWithId& B)
{
	const FValue& ValueA = A;
	const FValue& ValueB = B;
	const FValueId& IdA = A.GetId();
	const FValueId& IdB = B.GetId();
	return !(IdA == IdB) ? IdA < IdB : ValueA < ValueB;
}

} // UE::DerivedData

#undef UE_API
