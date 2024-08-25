// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeIndexTypes.generated.h"

/** uint16 index that can be invalid. */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeIndex16
{
	GENERATED_BODY()

	static constexpr uint16 InvalidValue = MAX_uint16;
	static const FStateTreeIndex16 Invalid;

	friend FORCEINLINE uint32 GetTypeHash(const FStateTreeIndex16 Index)
	{
		return GetTypeHash(Index.Value);
	}

	/** @return true if the given index can be represented by the type. */
	static bool IsValidIndex(const int32 Index)
	{
		return Index >= 0 && Index < (int32)MAX_uint16;
	}

	FStateTreeIndex16() = default;

	/**
	 * Construct from a uint16 index where MAX_uint16 is considered an invalid index
	 * (i.e FStateTreeIndex16::InvalidValue).
	 */
	explicit FStateTreeIndex16(const uint16 InIndex) : Value(InIndex)
	{
	}

	/**
	 * Construct from a int32 index where INDEX_NONE is considered an invalid index
	 * and converted to FStateTreeIndex16::InvalidValue (i.e MAX_uint16).
	 */
	explicit FStateTreeIndex16(const int32 InIndex)
	{
		check(InIndex == INDEX_NONE || IsValidIndex(InIndex));
		Value = InIndex == INDEX_NONE ? InvalidValue : (uint16)InIndex;
	}

	/** @return value of the index or FStateTreeIndex16::InvalidValue (i.e. MAX_uint16) if invalid. */
	uint16 Get() const { return Value; }
	
	/** @return the index value as int32, mapping invalid value to INDEX_NONE. */
	int32 AsInt32() const { return Value == InvalidValue ? INDEX_NONE : Value; }

	/** @return true if the index is valid. */
	bool IsValid() const { return Value != InvalidValue; }

	bool operator==(const FStateTreeIndex16& RHS) const { return Value == RHS.Value; }
	bool operator!=(const FStateTreeIndex16& RHS) const { return Value != RHS.Value; }

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

protected:
	UPROPERTY()
	uint16 Value = InvalidValue;
};

template<>
struct TStructOpsTypeTraits<FStateTreeIndex16> : public TStructOpsTypeTraitsBase2<FStateTreeIndex16>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

/** uint8 index that can be invalid. */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeIndex8
{
	GENERATED_BODY()

	static constexpr uint8 InvalidValue = MAX_uint8;
	static const FStateTreeIndex8 Invalid;
	
	/** @return true if the given index can be represented by the type. */
	static bool IsValidIndex(const int32 Index)
	{
		return Index >= 0 && Index < (int32)MAX_uint8;
	}
	
	FStateTreeIndex8() = default;
	
	explicit FStateTreeIndex8(const int32 InIndex)
	{
		check(InIndex == INDEX_NONE || IsValidIndex(InIndex));
		Value = InIndex == INDEX_NONE ? InvalidValue : (uint8)InIndex;
	}

	/** @return value of the index. */
	uint8 Get() const { return Value; }

	/** @return the index value as int32, mapping invalid value to INDEX_NONE. */
	int32 AsInt32() const { return Value == InvalidValue ? INDEX_NONE : Value; }
	
	/** @return true if the index is valid. */
	bool IsValid() const { return Value != InvalidValue; }

	bool operator==(const FStateTreeIndex8& RHS) const { return Value == RHS.Value; }
	bool operator!=(const FStateTreeIndex8& RHS) const { return Value != RHS.Value; }

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
	
protected:
	UPROPERTY()
	uint8 Value = InvalidValue;
};

template<>
struct TStructOpsTypeTraits<FStateTreeIndex8> : public TStructOpsTypeTraitsBase2<FStateTreeIndex8>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
