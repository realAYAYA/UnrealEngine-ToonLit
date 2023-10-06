// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAnimNextDecoratorHandle;

namespace UE::AnimNext
{
	/**
	 * Node Handle
	 * A node handle represents a reference to a specific node instance in the shared/read-only portion
	 * of a sub-graph. It points to a FNodeDescription when resolved.
	 * 
	 * @see FNodeDescription
	 */
	struct FNodeHandle
	{
		// Creates an invalid node handle
		FNodeHandle()
			: SharedOffset(INVALID_SHARED_OFFSET_VALUE)
		{}

		// Creates a node handle for the specified shared offset
		explicit FNodeHandle(uint32 SharedOffset_)
			: SharedOffset(SharedOffset_)
		{}

		// Returns true if this node handle is valid, false otherwise
		bool IsValid() const { return SharedOffset != INVALID_SHARED_OFFSET_VALUE; }

		// Returns the offset into the shared data relative to the root of the owning sub-graph
		uint32 GetSharedOffset() const { return SharedOffset; }

		// Equality and inequality tests
		bool operator==(FNodeHandle RHS) const { return SharedOffset == RHS.SharedOffset; }
		bool operator!=(FNodeHandle RHS) const { return SharedOffset != RHS.SharedOffset; }

	private:
		// We pack the shared offset on 24 bits, the top 8 bits are truncated
		static constexpr uint32 INVALID_SHARED_OFFSET_VALUE = ~0u >> (32 - 24);

		uint32	Padding : 8;			// unused padding
		uint32	SharedOffset : 24;		// relative to root of sub-graph

		friend FAnimNextDecoratorHandle;
	};
}
