// Copyright Epic Games, Inc. All Rights Reserved.

#include "PieFixupSerializer.h"

#include "Components/InstancedStaticMeshComponent.h"

class FMulticastDelegateProperty;

FPIEFixupSerializer::FPIEFixupSerializer(UObject* InRoot, int32 InPIEInstanceID)
	: SoftObjectPathFixupFunction([](int32, FSoftObjectPath&) {})
	, Root(InRoot)
	, PIEInstanceID(InPIEInstanceID)
{
	this->ArShouldSkipBulkData = true;
}

FPIEFixupSerializer::FPIEFixupSerializer(UObject* InRoot, int32 InPIEInstanceID, TFunctionRef<void(int32, FSoftObjectPath&)> InSoftObjectPathFixupFunction)
	: SoftObjectPathFixupFunction(InSoftObjectPathFixupFunction)
	, Root(InRoot)
	, PIEInstanceID(InPIEInstanceID)
{
	this->ArShouldSkipBulkData = true;
}

bool FPIEFixupSerializer::ShouldSkipProperty(const FProperty* InProperty) const
{
	return InProperty->IsA<FMulticastDelegateProperty>() || FArchiveUObject::ShouldSkipProperty(InProperty);
}

FArchive& FPIEFixupSerializer::operator<<(UObject*& Object)
{
	if (Object && (Object == Root ||Object->IsIn(Root)) && !VisitedObjects.Contains(Object))
	{
		VisitedObjects.Add(Object);

		// Skip instanced static mesh component as their impact on serialization is enormous and they don't contain lazy ptrs.
		if (!Cast<UInstancedStaticMeshComponent>(Object))
		{
			Object->Serialize(*this);
		}
	}
	return *this;
}

FArchive& FPIEFixupSerializer::operator<<(FSoftObjectPath& Value)
{
	Value.FixupForPIE(PIEInstanceID, SoftObjectPathFixupFunction);
	return *this;
}

FArchive& FPIEFixupSerializer::operator<<(FLazyObjectPtr& Value)
{
	Value.FixupForPIE(PIEInstanceID);
	return *this;
}

FArchive& FPIEFixupSerializer::operator<<(FSoftObjectPtr& Value)
{
	// Forward the serialization to the FSoftObjectPath overload so it can be fixed up
	*this << Value.GetUniqueID();
	return *this;
}