// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorWriter.h"

#if WITH_EDITOR
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "Serialization/ArchiveUObject.h"

#include "DecoratorBase/DecoratorReader.h"

namespace UE::AnimNext
{
	FDecoratorWriter::FDecoratorWriter()
		: FMemoryWriter(GraphSharedDataArchiveBuffer)
		, NextNodeID(FNodeID::GetFirstID())
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

		if (!NextNodeID.IsValid())
		{
			// We have too many nodes in the graph, we need to be able to represent them with 16 bits
			// The node ID must have wrapped around
			ErrorState = EErrorState::TooManyNodes;
			return FNodeHandle();
		}

		const FNodeHandle NodeHandle = FNodeHandle::FromNodeID(NextNodeID);
		check(NodeHandle.IsValid() && NodeHandle.IsNodeID());
		check(NodeMappings.Num() == NodeHandle.GetNodeID().GetNodeIndex());

		NextNodeID = NextNodeID.GetNextID();

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FNodeTemplateRegistryHandle NodeTemplateHandle = NodeTemplateRegistry.FindOrAdd(&NodeTemplate);

		NodeMappings.Add({ NodeHandle, NodeTemplateHandle, 0 });

		return NodeHandle;
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
		GraphReferencedObjects.Reset();

		// Serialize the node templates
		TArray<FNodeTemplateRegistryHandle> NodeTemplateHandles;
		NodeTemplateHandles.Reserve(NodeMappings.Num());

		for (FNodeMapping& NodeMapping : NodeMappings)
		{
			NodeMapping.NodeTemplateIndex = NodeTemplateHandles.AddUnique(NodeMapping.NodeTemplateHandle);
		}

		uint32 NumNodeTemplates = NodeTemplateHandles.Num();
		*this << NumNodeTemplates;

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		for (FNodeTemplateRegistryHandle NodeTemplateHandle : NodeTemplateHandles)
		{
			FNodeTemplate* NodeTemplate = NodeTemplateRegistry.FindMutable(NodeTemplateHandle);
			NodeTemplate->Serialize(*this);
		}

		// Begin serializing the graph shared data
		uint32 NumNodes = NodeMappings.Num();
		*this << NumNodes;

		// Serialize the node template indices that we'll use for each node
		for (const FNodeMapping& NodeMapping : NodeMappings)
		{
			uint32 NodeTemplateIndex = NodeMapping.NodeTemplateIndex;
			*this << NodeTemplateIndex;
		}
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

	void FDecoratorWriter::WriteNode(
		const FNodeHandle NodeHandle,
		const TFunction<FString (uint32 DecoratorIndex, FName PropertyName)>& GetDecoratorProperty,
		const TFunction<uint16(uint32 DecoratorIndex, FName PropertyName)>& GetDecoratorLatentPropertyIndex)
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

		FNodeDescription* NodeDesc = new(Buffer) FNodeDescription(NodeMapping->NodeHandle.GetNodeID(), NodeMapping->NodeTemplateHandle);

		// Populate our decorator properties
		const uint32 NumDecorators = NodeTemplate->GetNumDecorators();
		const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();
		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
		{
			const FDecoratorRegistryHandle DecoratorHandle = DecoratorTemplates[DecoratorIndex].GetRegistryHandle();
			const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorHandle);

			FAnimNextDecoratorSharedData* SharedData = DecoratorTemplates[DecoratorIndex].GetDecoratorDescription(*NodeDesc);

			// Curry our lambda with the decorator index
			const auto GetDecoratorPropertyAt = [&GetDecoratorProperty, DecoratorIndex](FName PropertyName)
			{
				return GetDecoratorProperty(DecoratorIndex, PropertyName);
			};

			Decorator->SaveDecoratorSharedData(GetDecoratorPropertyAt, *SharedData);
		}

		// Append our node and decorator shared data to our archive
		NodeDesc->Serialize(*this);

		// Append our decorator latent property handles to our archive
		// We only write out the properties that will be present at runtime
		// This takes into account editor only latent properties which can be stripped in cooked builds
		// Other forms of property stripping are not currently supported
		// The latent property offsets will be computed at runtime on load to support property sizes/alignment
		// changing between the editor and the runtime platform (e.g. 32 vs 64 bit pointers)
		// To that end, we serialize the following property metadata:
		//     * RigVM memory handle index
		//     * Whether the property supports freezing or not
		//     * The property name and index for us to look it up at runtime

		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
		{
			const FDecoratorRegistryHandle DecoratorHandle = DecoratorTemplates[DecoratorIndex].GetRegistryHandle();
			const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorHandle);

			// Curry our lambda with the decorator index
			const auto GetDecoratorLatentPropertyIndexAt = [&GetDecoratorLatentPropertyIndex, DecoratorIndex](FName PropertyName)
			{
				return GetDecoratorLatentPropertyIndex(DecoratorIndex, PropertyName);
			};

			TArray<FLatentPropertyMetadata> LatentProperties = Decorator->GetLatentPropertyHandles(IsFilterEditorOnly(), GetDecoratorLatentPropertyIndexAt);

			int32 NumLatentProperties = LatentProperties.Num();
			*this << NumLatentProperties;

			for (FLatentPropertyMetadata& Metadata : LatentProperties)
			{
				*this << Metadata;
			}
		}

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

	const TArray<UObject*>& FDecoratorWriter::GetGraphReferencedObjects() const
	{
		return GraphReferencedObjects;
	}

	FArchive& FDecoratorWriter::operator<<(UObject*& Obj)
	{
		// Add our object for tracking
		int32 ObjectIndex = GraphReferencedObjects.AddUnique(Obj);

		// Save our index, we'll use it to resolve the object on load
		*this << ObjectIndex;

		return *this;
	}

	FArchive& FDecoratorWriter::operator<<(FObjectPtr& Obj)
	{
		return FArchiveUObject::SerializeObjectPtr(*this, Obj);
	}
}
#endif
