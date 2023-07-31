// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyMapStructDeserializerBackendWrapper.h"
#include "UObject/UnrealType.h"

bool FPropertyMapStructDeserializerBackendWrapper::ReadProperty(FProperty* Property, FProperty* Outer, void* Data, int32 ArrayIndex)
{
	if (InnerBackend.ReadProperty(Property, Outer, Data, ArrayIndex))
	{
		FReadPropertyData ReadPropertyData;
		ReadPropertyData.Property = Property;
		ReadPropertyData.Data = Property->ContainerPtrToValuePtr<void>(Data, ArrayIndex);

		ReadProperties.Add(ReadPropertyData);
		
		return true;
	}

	return false;
}