// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorTemplate.h"

#include "Serialization/Archive.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"

namespace UE::AnimNext
{
	void FDecoratorTemplate::Serialize(FArchive& Ar)
	{
		Ar << UID;

		if (Ar.IsSaving())
		{
			const FDecorator* Decorator = FDecoratorRegistry::Get().Find(RegistryHandle);

			uint32 DecoratorUID = Decorator->GetDecoratorUID().GetUID();
			Ar << DecoratorUID;
		}
		else if (Ar.IsLoading())
		{
			uint32 DecoratorUID = 0;
			Ar << DecoratorUID;

			RegistryHandle = FDecoratorRegistry::Get().FindHandle(DecoratorUID);
		}
		else
		{
			// Counting, etc
			if (RegistryHandle.IsDynamic())
			{
				int32 DynamicIndex = RegistryHandle.GetDynamicIndex();
				Ar << DynamicIndex;
			}
			else if (RegistryHandle.IsStatic())
			{
				int32 StaticOffset = RegistryHandle.GetStaticOffset();
				Ar << StaticOffset;
			}
		}

		Ar << Mode;
		Ar << AdditiveIndexOrNumAdditive;
		Ar << NodeSharedOffset;
		Ar << NodeInstanceOffset;
	}
}
