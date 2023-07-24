// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDataStorageCompatibilityInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI)
class UTypedElementDataStorageCompatibilityInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface to provide compatibility with existing systems that don't directly
 * support the data storage.
 */
class TYPEDELEMENTFRAMEWORK_API ITypedElementDataStorageCompatibilityInterface
{
	GENERATED_BODY()

public:
	virtual void AddCompatibleObject(AActor* Actor) = 0;
	virtual void RemoveCompatibleObject(AActor* Actor) = 0;
};