// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Serialization/MemoryWriter.h"

#include "DecoratorBase/LatentPropertyHandle.h"
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
	class ANIMNEXT_API FDecoratorWriter final : public FMemoryWriter
	{
	public:
		enum class EErrorState
		{
			None,					// All good, no error
			TooManyNodes,			// Exceeded the maximum number of nodes in a graph, @see FNodeDescription::MAXIMUM_COUNT
			NodeTemplateNotFound,	// Failed to find a necessary node template
			NodeTemplateTooLarge,	// Exceeded the maximum node template size, @see FNodeTemplate::MAXIMUM_SIZE
			NodeHandleNotFound,		// Failed to find the mapping for a node handle, it was likely not registered
		};

		FDecoratorWriter();

		// Registers an instance of the provided node template and assigns a node handle and node UID to it
		[[nodiscard]] FNodeHandle RegisterNode(const FNodeTemplate& NodeTemplate);

		// Called before node writing can begin
		void BeginNodeWriting();

		// Called once node writing has terminated
		void EndNodeWriting();

		// Writes out the provided node using the decorator properties
		// Nodes must be written in the same order they were registered in
		void WriteNode(
			const FNodeHandle NodeHandle,
			const TFunction<FString(uint32 DecoratorIndex, FName PropertyName)>& GetDecoratorProperty,
			const TFunction<uint16(uint32 DecoratorIndex, FName PropertyName)>& GetDecoratorLatentPropertyIndex
			);

		// Returns the error state
		[[nodiscard]] EErrorState GetErrorState() const;

		// Returns the populated raw graph shared data buffer
		[[nodiscard]] const TArray<uint8>& GetGraphSharedData() const;

		// Returns the list of referenced UObjects in this graph
		[[nodiscard]] const TArray<UObject*>& GetGraphReferencedObjects() const;

		// FArchive implementation
		virtual FArchive& operator<<(UObject*& Obj) override;
		virtual FArchive& operator<<(FObjectPtr& Obj) override;

	private:
		struct FNodeMapping
		{
			// The node handle for this entry (encoded as a node ID)
			FNodeHandle NodeHandle;

			// The node template handle the node uses
			FNodeTemplateRegistryHandle NodeTemplateHandle;

			// The unique node template index that we'll serialize
			uint32 NodeTemplateIndex;
		};

		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<FNodeMapping> NodeMappings;

		// To track the node registration process
		FNodeID NextNodeID;

		// To track node writing
		TArray<UObject*> GraphReferencedObjects;
		uint32 NumNodesWritten;
		bool bIsNodeWriting;

		EErrorState ErrorState;
	};
}
#endif
