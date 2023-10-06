// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/NodeHandle.h"

#include "DecoratorHandle.generated.h"

/**
 * Decorator Handle
 * A decorator handle represents a reference to a specific decorator instance in the shared/read-only portion
 * of a sub-graph. It points to a FNodeDescription when resolved.
 * @see FNodeDescription
 */
USTRUCT()
struct FAnimNextDecoratorHandle
{
	GENERATED_BODY()

	// Creates an invalid decorator handle
	FAnimNextDecoratorHandle()
		: PackedDecoratorIndexAndNodeSharedOffset(INVALID_SHARED_OFFSET_VALUE)
	{}

	// Creates a decorator handle pointing to the first decorator of the specified node
	explicit FAnimNextDecoratorHandle(UE::AnimNext::FNodeHandle NodeHandle)
		: PackedDecoratorIndexAndNodeSharedOffset(NodeHandle.GetSharedOffset())
	{}

	// Creates a decorator handle pointing to the specified decorator on the specified node
	FAnimNextDecoratorHandle(UE::AnimNext::FNodeHandle NodeHandle, uint32 DecoratorIndex_)
		: PackedDecoratorIndexAndNodeSharedOffset(NodeHandle.GetSharedOffset() | (DecoratorIndex_ << 24))
	{}

	// Returns true if this decorator handle is valid, false otherwise
	bool IsValid() const { return (PackedDecoratorIndexAndNodeSharedOffset & SHARED_OFFSET_MASK) != INVALID_SHARED_OFFSET_VALUE; }

	// Returns the decorator index
	uint32 GetDecoratorIndex() const { return PackedDecoratorIndexAndNodeSharedOffset >> DECORATOR_SHIFT; }

	// Returns a handle to the node in the shared data segment
	UE::AnimNext::FNodeHandle GetNodeHandle() const { return UE::AnimNext::FNodeHandle(PackedDecoratorIndexAndNodeSharedOffset & SHARED_OFFSET_MASK); }

private:
	// Bottom 24 bits are used by the node shared offset while the top 8 bits by the decorator index
	static constexpr uint32 DECORATOR_SHIFT = 24;
	static constexpr uint32 SHARED_OFFSET_MASK = ~0u >> (32 - DECORATOR_SHIFT);
	static constexpr uint32 INVALID_SHARED_OFFSET_VALUE = SHARED_OFFSET_MASK;

	UPROPERTY()
	uint32		PackedDecoratorIndexAndNodeSharedOffset;
};
