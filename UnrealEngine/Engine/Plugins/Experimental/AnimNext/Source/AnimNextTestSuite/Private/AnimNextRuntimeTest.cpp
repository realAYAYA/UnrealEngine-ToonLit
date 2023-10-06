// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNextRuntimeTest.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"

namespace UE::AnimNext
{
	uint32 GetNodeTemplateUID(const TArray<FDecoratorUID>& NodeTemplateDecoratorList)
	{
		uint32 NodeTemplateUID = 0;
		for (const FDecoratorUID DecoratorUID : NodeTemplateDecoratorList)
		{
			NodeTemplateUID = HashCombineFast(NodeTemplateUID, DecoratorUID.GetUID());
		}

		return NodeTemplateUID;
	}

	uint32 GetNodeInstanceSize(const TArray<FDecoratorUID>& NodeTemplateDecoratorList)
	{
		const FDecoratorRegistry& Registry = FDecoratorRegistry::Get();

		uint32 NodeInstanceSize = sizeof(FNodeInstance);
		for (const FDecoratorUID DecoratorUID : NodeTemplateDecoratorList)
		{
			const FDecorator* Decorator = Registry.Find(DecoratorUID);

			const FDecoratorMemoryLayout MemoryLayout = Decorator->GetDecoratorMemoryDescription();
			NodeInstanceSize = Align(NodeInstanceSize, MemoryLayout.InstanceDataAlignment);
			NodeInstanceSize += MemoryLayout.InstanceDataSize;
		}

		return NodeInstanceSize;
	}

	void AppendTemplateDecorator(const TArray<FDecoratorUID>& NodeTemplateDecoratorList, int32 DecoratorIndex, TArray<uint8>& NodeTemplateBuffer, uint32& SharedDataOffset, uint32& InstanceDataOffset)
	{
		const FDecoratorRegistry& Registry = FDecoratorRegistry::Get();

		const FDecoratorUID DecoratorUID = NodeTemplateDecoratorList[DecoratorIndex];
		const FDecoratorRegistryHandle DecoratorHandle = Registry.FindHandle(DecoratorUID);
		const FDecorator* Decorator = Registry.Find(DecoratorHandle);
		const EDecoratorMode DecoratorMode = Decorator->GetDecoratorMode();

		uint32 AdditiveIndexOrNumAdditive;
		if (DecoratorMode == EDecoratorMode::Base)
		{
			// Find out how many additive decorators we have
			AdditiveIndexOrNumAdditive = 0;
			for (int32 Index = DecoratorIndex + 1; Index < NodeTemplateDecoratorList.Num(); ++Index)	// Skip ourself
			{
				const FDecorator* ChildDecorator = Registry.Find(NodeTemplateDecoratorList[Index]);
				if (ChildDecorator->GetDecoratorMode() == EDecoratorMode::Base)
				{
					break;	// Found another base decorator, we are done
				}

				// We are additive
				AdditiveIndexOrNumAdditive++;
			}
		}
		else
		{
			// Find out our additive index
			AdditiveIndexOrNumAdditive = 1;	// Skip ourself
			for (int32 Index = DecoratorIndex - 1; Index >= 0; --Index)
			{
				const FDecorator* ParentDecorator = Registry.Find(NodeTemplateDecoratorList[Index]);
				if (ParentDecorator->GetDecoratorMode() == EDecoratorMode::Base)
				{
					break;	// Found our base decorator, we are done
				}

				// We are additive
				AdditiveIndexOrNumAdditive++;
			}
		}

		const FDecoratorMemoryLayout MemoryLayout = Decorator->GetDecoratorMemoryDescription();

		// Align our data
		SharedDataOffset = Align(SharedDataOffset, MemoryLayout.SharedDataAlignment);
		InstanceDataOffset = Align(InstanceDataOffset, MemoryLayout.InstanceDataAlignment);

		// Append and update our offsets
		const int32 BufferIndex = NodeTemplateBuffer.AddUninitialized(sizeof(FDecoratorTemplate));
		new(&NodeTemplateBuffer[BufferIndex]) FDecoratorTemplate(DecoratorUID, DecoratorHandle, DecoratorMode, AdditiveIndexOrNumAdditive, SharedDataOffset, InstanceDataOffset);

		SharedDataOffset += MemoryLayout.SharedDataSize;
		InstanceDataOffset += MemoryLayout.InstanceDataSize;
	}

	const FNodeTemplate* BuildNodeTemplate(const TArray<FDecoratorUID>& NodeTemplateDecoratorList, TArray<uint8>& NodeTemplateBuffer)
	{
		const int32 BufferIndex = NodeTemplateBuffer.AddUninitialized(sizeof(FNodeTemplate));
		new(&NodeTemplateBuffer[BufferIndex]) FNodeTemplate(GetNodeTemplateUID(NodeTemplateDecoratorList), GetNodeInstanceSize(NodeTemplateDecoratorList), NodeTemplateDecoratorList.Num());

		uint32 SharedDataOffset = sizeof(FNodeDescription);
		uint32 InstanceDataOffset = sizeof(FNodeInstance);

		for (int32 DecoratorIndex = 0; DecoratorIndex < NodeTemplateDecoratorList.Num(); ++DecoratorIndex)
		{
			AppendTemplateDecorator(NodeTemplateDecoratorList, DecoratorIndex, NodeTemplateBuffer, SharedDataOffset, InstanceDataOffset);
		}

		return reinterpret_cast<const FNodeTemplate*>(&NodeTemplateBuffer[0]);
	}
}
#endif
