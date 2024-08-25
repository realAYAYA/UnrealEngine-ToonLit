// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/NodeHandle.h"

#include "DecoratorHandle.generated.h"

/**
 * Decorator Handle
 * A decorator handle represents a reference to a specific decorator instance in the shared/read-only portion
 * of a sub-graph. It points to a FNodeDescription when resolved.
 * 
 * Internally, it contains a node handle and a decorator index.
 * Decorator handles can only be used within an AnimNext graph serialized with FDecoratorWriter.
 * 
 * @see FNodeDescription, FNodeHandle
 */
USTRUCT(BlueprintType)
struct FAnimNextDecoratorHandle final
{
	GENERATED_BODY()

	// Creates an invalid decorator handle
	constexpr FAnimNextDecoratorHandle() noexcept
		: PackedDecoratorIndexAndNodeHandle(UE::AnimNext::FNodeHandle::INVALID_NODE_HANDLE_RAW_VALUE)
	{}

	// Creates a decorator handle pointing to the first decorator of the specified node
	explicit constexpr FAnimNextDecoratorHandle(UE::AnimNext::FNodeHandle NodeHandle) noexcept
		: PackedDecoratorIndexAndNodeHandle(NodeHandle.GetPackedValue() & NODE_HANDLE_MASK)
	{}

	// Creates a decorator handle pointing to the specified decorator on the specified node
	FAnimNextDecoratorHandle(UE::AnimNext::FNodeHandle NodeHandle, uint32 DecoratorIndex)
		: PackedDecoratorIndexAndNodeHandle((NodeHandle.GetPackedValue() & NODE_HANDLE_MASK) | (DecoratorIndex << 24))
	{
		check(DecoratorIndex <= MAX_uint8);	// Make sure we don't truncate
	}

	// Returns true if this decorator handle is valid, false otherwise
	constexpr bool IsValid() const noexcept { return GetNodeHandle().IsValid(); }

	// Returns the decorator index
	constexpr uint32 GetDecoratorIndex() const noexcept { return PackedDecoratorIndexAndNodeHandle >> DECORATOR_SHIFT; }

	// Returns a handle to the node referenced
	constexpr UE::AnimNext::FNodeHandle GetNodeHandle() const noexcept { return UE::AnimNext::FNodeHandle::FromPackedValue(PackedDecoratorIndexAndNodeHandle); }

	ANIMNEXT_API bool Serialize(class FArchive& Ar);

private:
	// Bottom 24 bits are used by the node handle while the top 8 bits by the decorator index
	static constexpr uint32 DECORATOR_SHIFT = 24;
	static constexpr uint32 NODE_HANDLE_MASK = ~0u >> (32 - DECORATOR_SHIFT);

	UPROPERTY()
	uint32		PackedDecoratorIndexAndNodeHandle;
};

template<>
struct TStructOpsTypeTraits<FAnimNextDecoratorHandle> : public TStructOpsTypeTraitsBase2<FAnimNextDecoratorHandle>
{
	enum
	{
		WithSerializer = true,
	};
};
