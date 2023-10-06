// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/NodeTemplateRegistryHandle.h"

#include <limits>

class FArchive;

namespace UE::AnimNext
{
	/**
	 * Node Description
	 * A node description represents a unique instance in the authored static graph.
	 * A node description may have any number of runtime instances in the dynamically executed graph.
	 * As such, a node description is read-only at runtime while a node instance is read/write.
	 * 
	 * A node description is followed in memory by the decorator descriptions (their shared read-only data)
	 * that live within it. Decorator descriptions include things like hard-coded/inline properties, pin links, etc.
	 * 
	 * A node description is itself an instance of a node template.
	 * 
	 * @see FNodeTemplate
	 */
	struct alignas(alignof(uint32)) FNodeDescription
	{
		// Largest allowed size for a node description
		// Graphs are currently limited to 64 KB because we use 16 bit offsets within our node/decorator handles
		// We have no particular limit on the node size besides the graph limit (for now)
		static constexpr uint32 MAXIMUM_SIZE = 64 * 1024;

		// The maximum number of nodes allowed within a single graph
		// We use 16 bits to represent node UIDs
		static constexpr uint32 MAXIMUM_COUNT = std::numeric_limits<uint16>::max();

		FNodeDescription(uint16 UID_, FNodeTemplateRegistryHandle TemplateHandle_)
			: UID(UID_)
			, TemplateHandle(TemplateHandle_)
		{}

		// Returns the node UID, unique to the owning sub-graph
		uint32 GetUID() const { return UID; }

		// Returns the handle of the node's template in the node template registry
		FNodeTemplateRegistryHandle GetTemplateHandle() const { return TemplateHandle; }

		// Serializes this node description instance and the shared data of each decorator that follows
		ANIMNEXT_API void Serialize(FArchive& Ar);

	private:
		uint16							UID;				// assigned during export/cook, unique to current sub-graph
		FNodeTemplateRegistryHandle		TemplateHandle;		// offset of the node template within the global list

		// Followed by a list of [FAnimNextDecoratorSharedData] instances and optional padding
	};
}
