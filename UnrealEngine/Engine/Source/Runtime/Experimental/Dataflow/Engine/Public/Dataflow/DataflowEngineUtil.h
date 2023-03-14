// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Math/Transform.h"
#include "ReferenceSkeleton.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"

namespace Dataflow
{
	namespace Reflection
	{
		template<class T>
		const T* FindObjectPtrProperty(const UObject* Owner, FName Name)
		{
			if (Owner && Owner->GetClass())
			{
				if (const ::FProperty* UEProperty = Owner->GetClass()->FindPropertyByName(Name))
				{
					if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(UEProperty))
					{
						if (const void* ObjectContainer = ObjectProperty->ContainerPtrToValuePtr<void>(Owner))
						{
							if (const UObject* Value = ObjectProperty->GetObjectPropertyValue(ObjectContainer))
							{
								return Cast<T>(Value);
							}
						}
					}
				}
			}
			return nullptr;
		}
	}

	namespace Animation
	{
		void DATAFLOWENGINE_API GlobalTransforms(const FReferenceSkeleton&, TArray<FTransform>& GlobalTransforms);
	}
}