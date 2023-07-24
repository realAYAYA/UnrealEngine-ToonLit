// Copyright Epic Games, Inc. All Rights Reserved.
#include "PhysicsEngine/PhysicsObjectBlueprintLibrary.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"

#include "Components/PrimitiveComponent.h"

FClosestPhysicsObjectResult UPhysicsObjectBlueprintLibrary::GetClosestPhysicsObjectFromWorldLocation(UPrimitiveComponent* Component, const FVector& WorldLocation)
{
	if (!Component)
	{
		return {};
	}

	TArray<Chaos::FPhysicsObject*> PhysicsObjects = Component->GetAllPhysicsObjects();
	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
	PhysicsObjects = PhysicsObjects.FilterByPredicate(
		// Maybe allow users to customize the predicate?
		[&Interface](Chaos::FPhysicsObject* Object) {
			// We need to filter so we only get the leaf bodies. Mainly in consideration of the geometry collection.
			return !Interface->HasChildren(Object);
		}
	);
	return Interface->GetClosestPhysicsBodyFromLocation(PhysicsObjects, WorldLocation);
}
bool UPhysicsObjectBlueprintLibrary::ExtractClosestPhysicsObjectResults(const FClosestPhysicsObjectResult& Result, FName& OutName)
{
	if (!Result)
	{
		return false;
	}
	OutName = Result.HitName();
	return true;
}
FTransform UPhysicsObjectBlueprintLibrary::GetPhysicsObjectWorldTransform(UPrimitiveComponent* Component, FName BoneName)
{
	if (!Component)
	{
		return FTransform::Identity;
	}

	if (Chaos::FPhysicsObject* PhysicsObject = Component->GetPhysicsObjectByName(BoneName))
	{
		FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead({&PhysicsObject, 1});
		return Interface->GetTransform(PhysicsObject);
	}
	
	return FTransform::Identity;
}
