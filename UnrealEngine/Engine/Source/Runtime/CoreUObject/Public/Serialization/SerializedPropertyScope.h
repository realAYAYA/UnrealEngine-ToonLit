// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncPackage.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

#include "Serialization/Archive.h"

/** Helper class to set and restore serialized property on an archive */
class FSerializedPropertyScope
{
	FArchive& Ar;
	FProperty* Property;
	COREUOBJECT_API void PushProperty();
	COREUOBJECT_API void PopProperty();
public:
	FSerializedPropertyScope(FArchive& InAr, FProperty* InProperty, const FProperty* OnlyIfOldProperty = nullptr)
		: Ar(InAr)
		, Property(InProperty)
	{
		if (!OnlyIfOldProperty || Ar.GetSerializedProperty() == OnlyIfOldProperty)
		{
			PushProperty();
		}
		else
		{
			Property = nullptr;
		}
	}
	~FSerializedPropertyScope()
	{
		PopProperty();
	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
