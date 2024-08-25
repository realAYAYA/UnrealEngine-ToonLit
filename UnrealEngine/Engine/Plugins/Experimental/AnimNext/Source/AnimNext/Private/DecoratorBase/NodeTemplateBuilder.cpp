// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeTemplateBuilder.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeTemplate.h"

namespace UE::AnimNext
{
	void FNodeTemplateBuilder::AddDecorator(FDecoratorUID DecoratorUID)
	{
		DecoratorUIDs.Add(DecoratorUID);
	}

	FNodeTemplate* FNodeTemplateBuilder::BuildNodeTemplate(TArray<uint8>& NodeTemplateBuffer) const
	{
		return BuildNodeTemplate(DecoratorUIDs, NodeTemplateBuffer);
	}

	FNodeTemplate* FNodeTemplateBuilder::BuildNodeTemplate(const TArray<FDecoratorUID>& InDecoratorUIDs, TArray<uint8>& NodeTemplateBuffer)
	{
		NodeTemplateBuffer.Reset();

		const uint32 NodeTemplateUID = GetNodeTemplateUID(InDecoratorUIDs);

		NodeTemplateBuffer.AddUninitialized(sizeof(FNodeTemplate));
		new(NodeTemplateBuffer.GetData()) FNodeTemplate(NodeTemplateUID, InDecoratorUIDs.Num());

		for (int32 DecoratorIndex = 0; DecoratorIndex < InDecoratorUIDs.Num(); ++DecoratorIndex)
		{
			AppendTemplateDecorator(InDecoratorUIDs, DecoratorIndex, NodeTemplateBuffer);
		}

		// Grab pointer after everything is setup, we could re-alloc
		FNodeTemplate* NodeTemplate = reinterpret_cast<FNodeTemplate*>(NodeTemplateBuffer.GetData());

		// Perform all our finalizing work
		NodeTemplate->Finalize();

		return NodeTemplate;
	}

	void FNodeTemplateBuilder::Reset()
	{
		DecoratorUIDs.Reset();
	}

	uint32 FNodeTemplateBuilder::GetNodeTemplateUID(const TArray<FDecoratorUID>& InDecoratorUIDs)
	{
		uint32 NodeTemplateUID = 0;

		for (const FDecoratorUID& DecoratorUID : InDecoratorUIDs)
		{
			NodeTemplateUID = HashCombineFast(NodeTemplateUID, DecoratorUID.GetUID());
		}

		return NodeTemplateUID;
	}

	void FNodeTemplateBuilder::AppendTemplateDecorator(
		const TArray<FDecoratorUID>& InDecoratorUIDs, int32 DecoratorIndex,
		TArray<uint8>& NodeTemplateBuffer)
	{
		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();

		const FDecoratorUID DecoratorUID = InDecoratorUIDs[DecoratorIndex];
		const FDecoratorRegistryHandle DecoratorHandle = DecoratorRegistry.FindHandle(DecoratorUID);
		const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorHandle);
		const EDecoratorMode DecoratorMode = Decorator->GetDecoratorMode();

		uint32 AdditiveIndexOrNumAdditive;
		if (DecoratorMode == EDecoratorMode::Base)
		{
			// Find out how many additive decorators we have
			AdditiveIndexOrNumAdditive = 0;
			for (int32 Index = DecoratorIndex + 1; Index < InDecoratorUIDs.Num(); ++Index)	// Skip ourself
			{
				const FDecorator* ChildDecorator = DecoratorRegistry.Find(InDecoratorUIDs[Index]);
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
				const FDecorator* ParentDecorator = DecoratorRegistry.Find(InDecoratorUIDs[Index]);
				if (ParentDecorator->GetDecoratorMode() == EDecoratorMode::Base)
				{
					break;	// Found our base decorator, we are done
				}

				// We are additive
				AdditiveIndexOrNumAdditive++;
			}
		}

		// Append our decorator template
		const int32 BufferIndex = NodeTemplateBuffer.AddUninitialized(sizeof(FDecoratorTemplate));
		new(&NodeTemplateBuffer[BufferIndex]) FDecoratorTemplate(DecoratorUID, DecoratorHandle, DecoratorMode, AdditiveIndexOrNumAdditive);
	}
}
