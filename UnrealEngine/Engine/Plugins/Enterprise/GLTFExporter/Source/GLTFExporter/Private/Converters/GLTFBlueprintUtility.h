// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"

class AActor;

struct FGLTFBlueprintUtility
{
	static FString GetClassPath(const AActor* Actor);

	static bool IsSkySphere(const FString& Path);

	static bool IsHDRIBackdrop(const FString& Path);

	template <class ValueType>
	static bool TryGetPropertyValue(const UObject* Object, const TCHAR* PropertyName, ValueType& OutValue)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		if (Property == nullptr)
		{
			return false;
		}

		const ValueType* ValuePtr = Property->ContainerPtrToValuePtr<ValueType>(Object);
		if (ValuePtr == nullptr)
		{
			return false;
		}

		OutValue = *ValuePtr;
		return true;
	}

	static bool TryGetPropertyValue(const UObject* Object, const TCHAR* PropertyName, float& OutValue)
	{
		// NOTE: blueprints always uses double instead of float in UE5

		double Value;
		if (!TryGetPropertyValue(Object, PropertyName, Value))
		{
			return false;
		}

		OutValue = static_cast<float>(Value);
		return true;
	}
};
