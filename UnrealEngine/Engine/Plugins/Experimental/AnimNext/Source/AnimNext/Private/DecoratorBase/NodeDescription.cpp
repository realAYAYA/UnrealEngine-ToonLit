// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeDescription.h"

#include "Serialization/Archive.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"

namespace UE::AnimNext
{
	void FNodeDescription::Serialize(FArchive& Ar)
	{
		Ar << UID;

		if (Ar.IsSaving())
		{
			const FNodeTemplate* NodeTemplate = FNodeTemplateRegistry::Get().Find(TemplateHandle);

			uint32 TemplateUID = NodeTemplate->GetUID();
			Ar << TemplateUID;
		}
		else if (Ar.IsLoading())
		{
			uint32 TemplateUID = 0;
			Ar << TemplateUID;

			TemplateHandle = FNodeTemplateRegistry::Get().Find(TemplateUID);
		}
		else
		{
			// Counting, etc
			int32 TemplateOffset = TemplateHandle.GetTemplateOffset();
			Ar << TemplateOffset;
		}

		// Use our template to serialize our decorators
		const FNodeTemplate* NodeTemplate = FNodeTemplateRegistry::Get().Find(TemplateHandle);

		const uint32 NumDecorators = NodeTemplate->GetNumDecorators();
		const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();
		for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
		{
			const FDecoratorRegistryHandle DecoratorHandle = DecoratorTemplates[DecoratorIndex].GetRegistryHandle();
			const FDecorator* Decorator = FDecoratorRegistry::Get().Find(DecoratorHandle);

			FAnimNextDecoratorSharedData* SharedData = DecoratorTemplates[DecoratorIndex].GetDecoratorDescription(*this);

			Decorator->SerializeDecoratorSharedData(Ar, *SharedData);
		}
	}
}
