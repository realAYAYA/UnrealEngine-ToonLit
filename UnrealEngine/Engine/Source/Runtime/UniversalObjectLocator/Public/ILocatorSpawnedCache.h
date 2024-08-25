// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UniversalObjectLocatorParameterTypeHandle.h"
#include "Templates/SharedPointer.h"
#include "ILocatorSpawnedCache.generated.h"

namespace UE::UniversalObjectLocator
{

/* An interface for a basic cache used by locators that might create an object or spawn an actor during 'Load'
* and destroy/unspawn it during 'Unload'.
* Users of a UOL may choose to use a cache to control multiplicity of spawned objects/actors using the same UOL- for example
* to ensure only 1 is created per UOL per context, or to allow multiple.
* To use the cache, the resolver of the UOL must pass it as an argument in FResolveParams.
*/
struct UNIVERSALOBJECTLOCATOR_API ILocatorSpawnedCache
{
	virtual ~ILocatorSpawnedCache() {}

	// Find the existing object spawned for the current UOL.
	virtual UObject* FindExistingObject() = 0;

	// For objects or actors that may be created or spawned, the resolver can request a name for them to be called on Spawn.
	virtual FName GetRequestedObjectName() { return FName(); }

	// If an object is spawned by a UOL during 'Load', this should be called to register the object with the cache.
	virtual void ReportSpawnedObject(UObject* Object) = 0;

	// If an object is despawned by a UOL during 'Unload' this should be called to unregister the object.
	virtual void SpawnedObjectDestroyed() = 0;
};

}; // namespace UE::UniversalObjectLocator


USTRUCT()
struct UNIVERSALOBJECTLOCATOR_API FLocatorSpawnedCacheResolveParameter
{
	GENERATED_BODY()

	static UE::UniversalObjectLocator::TParameterTypeHandle<FLocatorSpawnedCacheResolveParameter> ParameterType;

	mutable UE::UniversalObjectLocator::ILocatorSpawnedCache* Cache = nullptr;
};