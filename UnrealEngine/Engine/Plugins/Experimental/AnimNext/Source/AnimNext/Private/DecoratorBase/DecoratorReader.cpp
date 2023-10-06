// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorReader.h"

#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"

namespace UE::AnimNext
{
	FDecoratorReader::FDecoratorReader(FArchive& Ar)
		: FArchiveProxy(Ar)
	{
	}

	void FDecoratorReader::ReadGraphSharedData(TArray<uint8>& GraphSharedData)
	{
		GraphSharedData.Empty(0);

		// Read the node templates and register them as needed
		uint32 NumNodeTemplates = 0;
		*this << NumNodeTemplates;

		// We serialize each node template in the same buffer, they get copied into the registry and we don't need to retain them
		alignas(16) uint8 NodeTemplateBuffer[64 * 1024];
		FNodeTemplate* NodeTemplate = reinterpret_cast<FNodeTemplate*>(&NodeTemplateBuffer[0]);

		FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		for (uint32 NodeTemplateIndex = 0; NodeTemplateIndex < NumNodeTemplates; ++NodeTemplateIndex)
		{
			NodeTemplate->Serialize(*this);

			// Register our node template
			NodeTemplateRegistry.FindOrAdd(NodeTemplate);
		}

		// Read our graph shared data
		uint32 NumNodes = 0;
		*this << NumNodes;

		uint32 SharedDataSize = 0;
		*this << SharedDataSize;

		GraphSharedData.AddZeroed(SharedDataSize);

		uint32 SharedDataOffset = 0;
		for (uint32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
		{
			uint32 NodeSize = 0;
			*this << NodeSize;

			FNodeDescription* NodeDesc = reinterpret_cast<FNodeDescription*>(&GraphSharedData[SharedDataOffset]);
			NodeDesc->Serialize(*this);

			SharedDataOffset += NodeSize;
		}

		ensure(SharedDataOffset == SharedDataSize);
	}
}
