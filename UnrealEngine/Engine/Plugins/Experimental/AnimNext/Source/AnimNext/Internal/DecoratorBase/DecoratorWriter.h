// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Serialization/MemoryWriter.h"

#include "DecoratorBase/NodeHandle.h"
#include "DecoratorBase/NodeTemplateRegistryHandle.h"

namespace UE::AnimNext
{
	struct FNodeTemplate;

	/**
	  * FDecoratorWriter
	  *
	  * The decorator writer is used to write a serialized binary blob that contains
	  * the anim graph data. An anim graph contains the following:
	  *     - A list of FNodeTemplates that the nodes use
	  *     - The graph shared data (FNodeDescription for every node)
	  */
	class ANIMNEXT_API FDecoratorWriter : public FMemoryWriter
	{
	public:
		// The largest size allowed for the shared data of a single graph
		// We use 16 bit offsets to index into the shared data
		static constexpr uint32 MAXIMUM_GRAPH_SHARED_DATA_SIZE = 64 * 1024;

		enum class EErrorState
		{
			None,					// All good, no error
			TooManyNodes,			// Exceeded the maximum number of nodes in a graph, @see FNodeDescription::MAXIMUM_COUNT
			GraphTooLarge,			// Exceeded the maximum graph size, @see FDecoratorWriter::MAXIMUM_GRAPH_SHARED_DATA_SIZE
			NodeTemplateNotFound,	// Failed to find a necessary node template
			NodeTemplateTooLarge,	// Exceeded the maximum node template size, @see FNodeTemplate::MAXIMUM_SIZE
			NodeSharedDataTooLarge,	// Exceeded the maximum node shared data size, @see FNodeDescription::MAXIMUM_SIZE
			NodeHandleNotFound,		// Failed to find the mapping for a node handle, it was likely not registered
		};

		FDecoratorWriter();

		// Registers an instance of the provided node template and assigns a node handle and node UID to it
		FNodeHandle RegisterNode(const FNodeTemplate& NodeTemplate);

		// Returns the node UID associated with a node handle
		uint16 GetNodeUID(const FNodeHandle NodeHandle) const;

		// Returns the node handle associated with a node UID
		FNodeHandle GetNodeHandle(uint16 NodeUID) const;

		// Called before node writing can begin
		void BeginNodeWriting();

		// Called once node writing has terminated
		void EndNodeWriting();

		// Writes out the provided node using the decorator properties
		// Nodes must be written in the same order they were registered in
		void WriteNode(const FNodeHandle NodeHandle, const TFunction<const TMap<FString, FString>& (uint32 DecoratorIndex)>& GetDecoratorProperties);

		// Returns the error state
		EErrorState GetErrorState() const;

		// Returns the populated raw graph shared data buffer
		const TArray<uint8>& GetGraphSharedData() const;

	private:
		struct FNodeMapping
		{
			FNodeHandle NodeHandle;
			uint32 NodeSize;
			uint16 NodeUID;
			FNodeTemplateRegistryHandle NodeTemplateHandle;
		};

		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<FNodeMapping> NodeMappings;

		// To track the node registration process
		uint32 GraphSharedDataSize;
		uint16 NextNodeUID;

		// To track node writing
		uint32 NumNodesWritten;
		bool bIsNodeWriting;

		EErrorState ErrorState;
	};
}
#endif
