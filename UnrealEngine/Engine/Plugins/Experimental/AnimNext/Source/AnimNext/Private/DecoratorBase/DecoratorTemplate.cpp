// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorTemplate.h"

#include "Serialization/Archive.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"

namespace UE::AnimNext
{
	void FDecoratorTemplate::Serialize(FArchive& Ar)
	{
		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();

		Ar << UID;

		if (Ar.IsSaving())
		{
			const FDecorator* Decorator = DecoratorRegistry.Find(RegistryHandle);

			uint32 DecoratorUID = Decorator->GetDecoratorUID().GetUID();
			Ar << DecoratorUID;
		}
		else if (Ar.IsLoading())
		{
			uint32 DecoratorUID = 0;
			Ar << DecoratorUID;

			// It is possible that we fail to find the decorator that we need
			// This can happen if the decorator hasn't been loaded or registered
			// When this happens, the decorator is a no-op and the runtime behavior
			// may not be what is expected
			RegistryHandle = DecoratorRegistry.FindHandle(FDecoratorUID(DecoratorUID));
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
	}
}
