// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorTemplate.h"

#include <type_traits>

class FArchive;

namespace UE::AnimNext
{
	/**
	 * Node Template
	 * A node template represents a specific combination and ordering of decorators on a node.
	 * For example, a sequence player + sync group decorator + output caching decorator
	 * 
	 * Multiple node descriptions can share the same template each with their own property/pin values.
	 * 
	 * Templates can exist in multiple sub-graphs on disk. On load, a single unique copy is retained in
	 * memory and shared between ALL graphs.
	 * 
	 * @see FNodeDescription
	 */
	struct FNodeTemplate
	{
		// The largest size allowed for a node template
		// Node templates are just a lightweight descriptor, in theory they can have any size
		// We artificially specify a conservative upper bound
		static constexpr uint32 MAXIMUM_SIZE = 64 * 1024;

		FNodeTemplate(uint32 UID_, uint32 InstanceSize_, uint32 NumDecorators_)
			: UID(UID_)
			, InstanceSize(InstanceSize_)
			, NumDecorators(NumDecorators_)
		{}

		// Returns the globally unique template identifier (a hash of all the decorator UIDs present in this node)
		uint32 GetUID() const { return UID; }

		// Returns the size in bytes of this node template
		uint32 GetNodeTemplateSize() const { return sizeof(FNodeTemplate) + NumDecorators * sizeof(FDecoratorTemplate); }

		// Returns the size in bytes of a node instance
		uint32 GetInstanceSize() const { return InstanceSize; }

		// Returns the number of decorators present in the node template
		uint32 GetNumDecorators() const { return NumDecorators; }

		// Returns a pointer to the list of decorator template descriptions
		FDecoratorTemplate* GetDecorators() { return reinterpret_cast<FDecoratorTemplate*>(reinterpret_cast<uint8*>(this) + sizeof(FNodeTemplate)); }

		// Returns a pointer to the list of decorator template descriptions
		const FDecoratorTemplate* GetDecorators() const { return reinterpret_cast<const FDecoratorTemplate*>(reinterpret_cast<const uint8*>(this) + sizeof(FNodeTemplate)); }

		// Serializes this node template instance and each decorator template that follows
		ANIMNEXT_API void Serialize(FArchive& Ar);

	private:
		uint32	UID;				// globally unique template identifier or hash

		uint16	InstanceSize;		// size in bytes of a node instance

		uint8	NumDecorators;
		uint8	Padding[1];

		// TODO: We could use the padding (and extra space) here to cache which decorator handles which interface
		// This would avoid the need to iterate on every decorator to look it up. Perhaps only common interfaces could be cached.

		// Followed by a list of [FDecoratorTemplate] instances
	};

	static_assert(std::is_trivially_copyable_v<FNodeTemplate>, "FNodeTemplate needs to be trivially copyable");
	static_assert(alignof(FNodeTemplate) == alignof(FDecoratorTemplate), "FNodeTemplate and FDecoratorTemplate must have the same alignment");
}
