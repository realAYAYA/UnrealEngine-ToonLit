// Copyright Epic Games, Inc. All Rights Reserved.

#include "PieFixupSerializer.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

class FMulticastDelegateProperty;

namespace
{
	void DefaultSoftObjectPathFixupFunction(int32, FSoftObjectPath&) {}
}

FPIEFixupSerializer::FPIEFixupSerializer(UObject* InRoot, int32 InPIEInstanceID)
	: SoftObjectPathFixupFunction(DefaultSoftObjectPathFixupFunction)
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

#if WITH_EDITOR
		if (UPackage* ExternalPackage = Object->GetExternalPackage())
		{
			check(Object->IsPackageExternal());
			check(ExternalPackage->HasAnyPackageFlags(PKG_PlayInEditor));
			ExternalPackage->SetPIEInstanceID(PIEInstanceID);
		}
#endif

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
