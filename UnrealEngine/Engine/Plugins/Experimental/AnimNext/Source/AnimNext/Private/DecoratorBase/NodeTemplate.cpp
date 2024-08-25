// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeTemplate.h"

#include "Serialization/Archive.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"

namespace UE::AnimNext
{
	namespace Private
	{
		static uint32 GetNumSubStackLatentProperties(const FDecoratorRegistry& DecoratorRegistry, const FDecoratorTemplate* DecoratorTemplates, uint32 NumDecorators, uint32 BaseDecoratorIndex)
		{
			const FDecoratorTemplate& BaseDecoratorTemplate = DecoratorTemplates[BaseDecoratorIndex];
			check(BaseDecoratorTemplate.GetMode() == EDecoratorMode::Base);

			const uint32 NumSubStackDecorators = BaseDecoratorTemplate.GetNumAdditiveDecorators() + 1;
			uint32 NumSubStackLatentProperties = 0;

			for (uint32 SubStackDecoratorIndex = 0; SubStackDecoratorIndex < NumSubStackDecorators; ++SubStackDecoratorIndex)
			{
				const uint32 DecoratorIndex = BaseDecoratorIndex + SubStackDecoratorIndex;
				check(DecoratorIndex < NumDecorators);

				const FDecoratorTemplate& DecoratorTemplate = DecoratorTemplates[DecoratorIndex];
				const FDecoratorUID DecoratorUID = DecoratorTemplate.GetUID();

				if (const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorUID))
				{
					NumSubStackLatentProperties += Decorator->GetNumLatentDecoratorProperties();
				}
			}

			return NumSubStackLatentProperties;
		}
	}

	void FNodeTemplate::Serialize(FArchive& Ar)
	{
		Ar << UID;
		Ar << NumDecorators;

		const uint32 NumDecorators_ = GetNumDecorators();
		FDecoratorTemplate* DecoratorTemplates = GetDecorators();

		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators_; ++DecoratorIndex)
		{
			DecoratorTemplates[DecoratorIndex].Serialize(Ar);
		}

		if (Ar.IsLoading())
		{
			// When loading, make sure to recompute all runtime dependent values (e.g. sizes and offsets)
			Finalize();
		}
	}

	void FNodeTemplate::Finalize()
	{
		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();

		const uint32 NumDecorators_ = GetNumDecorators();
		FDecoratorTemplate* DecoratorTemplates = GetDecorators();

		uint32 SharedDataOffset = sizeof(FNodeDescription);
		uint32 InstanceDataOffset = sizeof(FNodeInstance);
		uint32 SharedLatentPropertyHandlesOffset = 0;

		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators_; ++DecoratorIndex)
		{
			FDecoratorTemplate& DecoratorTemplate = DecoratorTemplates[DecoratorIndex];
			const FDecoratorUID DecoratorUID = DecoratorTemplate.GetUID();

			uint32 NumLatentProperties = 0;
			uint32 DecoratorSharedDataOffset = 0;
			uint32 DecoratorSharedLatentPropertyHandlesOffset = 0;
			uint32 DecoratorInstanceDataOffset = 0;	// For instance data, 0 is an invalid offset since the data follows an instance of FNodeInstance

			uint32 NumSubStackLatentProperties = 0;
			if (DecoratorTemplate.GetMode() == EDecoratorMode::Base)
			{
				NumSubStackLatentProperties = Private::GetNumSubStackLatentProperties(DecoratorRegistry, DecoratorTemplates, NumDecorators_, DecoratorIndex);
			}
			// Skip decorators that we can't find
			// If a decorator isn't loaded and we attempt to run the graph, it will be a no-op entry
			if (const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorUID))
			{
				const FDecoratorMemoryLayout MemoryLayout = Decorator->GetDecoratorMemoryDescription();

				// Align our data
				SharedDataOffset = Align(SharedDataOffset, MemoryLayout.SharedDataAlignment);
				InstanceDataOffset = Align(InstanceDataOffset, MemoryLayout.InstanceDataAlignment);

				// Save our decorator offsets
				DecoratorSharedDataOffset = SharedDataOffset;
				DecoratorInstanceDataOffset = InstanceDataOffset;

				// Include our decorator
				SharedDataOffset += MemoryLayout.SharedDataSize;
				InstanceDataOffset += MemoryLayout.InstanceDataSize;

				// Base decorators include the list of all latent property handles in its shared data
				// Latent property offsets will point into that list
				if (DecoratorTemplate.GetMode() == EDecoratorMode::Base && NumSubStackLatentProperties != 0)
				{
					// Align our handles
					SharedDataOffset = Align(SharedDataOffset, alignof(FLatentPropertiesHeader));

					// Save the offset where we start, we'll increment it as we consume it
					SharedLatentPropertyHandlesOffset = SharedDataOffset;

					// Include the handles in the shared data and their header
					SharedDataOffset += sizeof(FLatentPropertiesHeader) + (NumSubStackLatentProperties * sizeof(FLatentPropertyHandle));

					// Skip the header
					SharedLatentPropertyHandlesOffset += sizeof(FLatentPropertiesHeader);
				}

				// Save our latent pins offset (if we have any)
				NumLatentProperties = Decorator->GetNumLatentDecoratorProperties();
				if (NumLatentProperties != 0)
				{
					DecoratorSharedLatentPropertyHandlesOffset = SharedLatentPropertyHandlesOffset;
					SharedLatentPropertyHandlesOffset += NumLatentProperties * sizeof(FLatentPropertyHandle);
				}
			}

			check(NumSubStackLatentProperties <= MAX_uint16);
			check(NumLatentProperties <= MAX_uint16);

			DecoratorTemplate.NumLatentProperties = static_cast<uint16>(NumLatentProperties);
			DecoratorTemplate.NumSubStackLatentProperties = static_cast<uint16>(NumSubStackLatentProperties);

			check(DecoratorSharedDataOffset <= MAX_uint16);
			check(DecoratorSharedLatentPropertyHandlesOffset <= MAX_uint16);
			check(DecoratorInstanceDataOffset <= MAX_uint16);

			// Update our decorator offsets
			DecoratorTemplate.NodeSharedOffset = static_cast<uint16>(DecoratorSharedDataOffset);
			DecoratorTemplate.NodeSharedLatentPropertyHandlesOffset = static_cast<uint16>(DecoratorSharedLatentPropertyHandlesOffset);
			DecoratorTemplate.NodeInstanceOffset = static_cast<uint16>(DecoratorInstanceDataOffset);
		}

		// Make sure we respect our alignment constraints
		SharedDataOffset = Align(SharedDataOffset, alignof(FNodeDescription));

		// Our size is the offset of the decorator that would follow afterwards
		// If the size is too large, we'll end up truncating the offsets/size
		// Set a value of 0 to be able to detect it later
		NodeSharedDataSize = SharedDataOffset > MAX_uint16 ? 0 : static_cast<uint16>(SharedDataOffset);
		NodeInstanceDataSize = InstanceDataOffset > MAX_uint16 ? 0 : static_cast<uint16>(InstanceDataOffset);
	}
}
