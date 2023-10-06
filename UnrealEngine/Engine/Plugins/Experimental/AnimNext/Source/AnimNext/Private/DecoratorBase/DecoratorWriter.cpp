// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorWriter.h"

#if WITH_EDITOR
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"

namespace UE::AnimNext
{
	namespace Private
	{
		// Note: this does not account for inline/pooled/pinned properties
		static uint32 GetNodeSharedSize(const FNodeTemplate& NodeTemplate)
		{
			const FDecoratorRegistry& Registry = FDecoratorRegistry::Get();

			const uint32 NumDecorators = NodeTemplate.GetNumDecorators();
			const FDecoratorTemplate* DecoratorTemplates = NodeTemplate.GetDecorators();

			uint32 NodeSharedSize = sizeof(FNodeDescription);
			for (uint32_t DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
			{
				const FDecorator* Decorator = Registry.Find(DecoratorTemplates[DecoratorIndex].GetRegistryHandle());

				const FDecoratorMemoryLayout MemoryLayout = Decorator->GetDecoratorMemoryDescription();
				NodeSharedSize = Align(NodeSharedSize, MemoryLayout.SharedDataAlignment);
				NodeSharedSize += MemoryLayout.SharedDataSize;
			}

			return NodeSharedSize;
		}
	}

	FDecoratorWriter::FDecoratorWriter()
		: FMemoryWriter(GraphSharedDataArchiveBuffer)
		, GraphSharedDataSize(0)
		, NextNodeUID(1)	// Node UID 0 is reserved for the invalid value (null)
		, NumNodesWritten(0)
		, bIsNodeWriting(false)
		, ErrorState(EErrorState::None)
	{
	}

	FNodeHandle FDecoratorWriter::RegisterNode(const FNodeTemplate& NodeTemplate)
	{
		ensure(!bIsNodeWriting);

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return FNodeHandle();
		}

		if (NodeTemplate.GetNodeTemplateSize() > FNodeTemplate::MAXIMUM_SIZE)
		{
			// This node template is too large
			ErrorState = EErrorState::NodeTemplateTooLarge;
			return FNodeHandle();
		}

		if (NextNodeUID == FNodeDescription::MAXIMUM_COUNT)
		{
			// We have too many nodes in the graph, we need to be able to represent them with 16 bits
			ErrorState = EErrorState::TooManyNodes;
			return FNodeHandle();
		}

		const uint32 NodeSharedDataSize = Private::GetNodeSharedSize(NodeTemplate);
		if (NodeSharedDataSize > FNodeDescription::MAXIMUM_SIZE)
		{
			// This node shared data is too large
			ErrorState = EErrorState::NodeSharedDataTooLarge;
			return FNodeHandle();
		}

		const uint32 NodeSharedDataOffset = GraphSharedDataSize;
		if (GraphSharedDataSize + NodeSharedDataSize > MAXIMUM_GRAPH_SHARED_DATA_SIZE)
		{
			// The graph shared data size is too large
			ErrorState = EErrorState::GraphTooLarge;
			return FNodeHandle();
		}

		GraphSharedDataSize += NodeSharedDataSize;

		const FNodeHandle NodeHandle(NodeSharedDataOffset);
		const uint16 NodeUID = NextNodeUID++;

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FNodeTemplateRegistryHandle NodeTemplateHandle = NodeTemplateRegistry.FindOrAdd(&NodeTemplate);

		NodeMappings.Add({ NodeHandle, NodeSharedDataSize, NodeUID, NodeTemplateHandle });

		return NodeHandle;
	}

	uint16 FDecoratorWriter::GetNodeUID(const FNodeHandle NodeHandle) const
	{
		const FNodeMapping* NodeMapping = NodeMappings.FindByPredicate([NodeHandle](const FNodeMapping& It) { return It.NodeHandle == NodeHandle; });
		return NodeMapping != nullptr ? NodeMapping->NodeUID : 0;
	}

	FNodeHandle FDecoratorWriter::GetNodeHandle(uint16 NodeUID) const
	{
		const FNodeMapping* NodeMapping = NodeMappings.FindByPredicate([NodeUID](const FNodeMapping& It) { return It.NodeUID == NodeUID; });
		return NodeMapping != nullptr ? NodeMapping->NodeHandle : FNodeHandle();
	}

	void FDecoratorWriter::BeginNodeWriting()
	{
		ensure(!bIsNodeWriting);
		ensure(NumNodesWritten == 0);

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return;
		}

		bIsNodeWriting = true;

		// Serialize the node templates
		TArray<FNodeTemplateRegistryHandle> NodeTemplateHandles;
		NodeTemplateHandles.Reserve(NodeMappings.Num());

		for (const FNodeMapping& NodeMapping : NodeMappings)
		{
			NodeTemplateHandles.AddUnique(NodeMapping.NodeTemplateHandle);
		}

		uint32 NumNodeTemplates = NodeTemplateHandles.Num();
		*this << NumNodeTemplates;

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		for (FNodeTemplateRegistryHandle NodeTemplateHandle : NodeTemplateHandles)
		{
			FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeTemplateHandle);
			NodeTemplate->Serialize(*this);
		}

		// Begin serializing the graph shared data
		uint32 NumNodes = NodeMappings.Num();
		*this << NumNodes;

		uint32 SharedDataSize = GraphSharedDataSize;
		*this << SharedDataSize;
	}

	void FDecoratorWriter::EndNodeWriting()
	{
		ensure(bIsNodeWriting);
		bIsNodeWriting = false;

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return;
		}

		ensure(NumNodesWritten == NodeMappings.Num());
	}

	void FDecoratorWriter::WriteNode(const FNodeHandle NodeHandle, const TFunction<const TMap<FString, FString>& (uint32 DecoratorIndex)>& GetDecoratorProperties)
	{
		ensure(bIsNodeWriting);

		if (ErrorState != EErrorState::None)
		{
			// We encountered an error, do nothing
			return;
		}

		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();
		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();

		const FNodeMapping* NodeMapping = NodeMappings.FindByPredicate([NodeHandle](const FNodeMapping& It) { return It.NodeHandle == NodeHandle; });
		if (NodeMapping == nullptr)
		{
			ErrorState = EErrorState::NodeHandleNotFound;
			return;
		}

		const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeMapping->NodeTemplateHandle);
		if (NodeTemplate == nullptr)
		{
			ErrorState = EErrorState::NodeTemplateNotFound;
			return;
		}

		// Populate our node description into a temporary buffer
		alignas(16) uint8 Buffer[64 * 1024];	// Max node size

		FNodeDescription* NodeDesc = new(Buffer) FNodeDescription(NodeMapping->NodeUID, NodeMapping->NodeTemplateHandle);

		// Populate our decorator properties
		const uint32 NumDecorators = NodeTemplate->GetNumDecorators();
		const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();
		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
		{
			const FDecoratorRegistryHandle DecoratorHandle = DecoratorTemplates[DecoratorIndex].GetRegistryHandle();
			const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorHandle);

			FAnimNextDecoratorSharedData* SharedData = DecoratorTemplates[DecoratorIndex].GetDecoratorDescription(*NodeDesc);

			const TMap<FString, FString>& Properties = GetDecoratorProperties(DecoratorIndex);
			Decorator->SaveDecoratorSharedData(*this, Properties, *SharedData);
		}

		// Write our node size first so the reader knows how much to skip ahead to the next node
		uint32 NodeSize = NodeMapping->NodeSize;
		*this << NodeSize;

		// Append it to our archive
		NodeDesc->Serialize(*this);

		NumNodesWritten++;
	}

	FDecoratorWriter::EErrorState FDecoratorWriter::GetErrorState() const
	{
		return ErrorState;
	}

	const TArray<uint8>& FDecoratorWriter::GetGraphSharedData() const
	{
		return GraphSharedDataArchiveBuffer;
	}
}
#endif
