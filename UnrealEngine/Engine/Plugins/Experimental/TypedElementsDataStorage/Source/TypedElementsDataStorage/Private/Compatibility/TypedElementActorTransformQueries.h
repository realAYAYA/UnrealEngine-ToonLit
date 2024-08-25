// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorTransformQueries.generated.h"

UCLASS()
class UTypedElementActorTransformFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementActorTransformFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	/**
	 * Checks actors that don't have a tranform column and adds one if an actor has been
	 * assigned a transform.
	 */
	void RegisterActorAddTransformColumn(ITypedElementDataStorageInterface& DataStorage) const;
	/**
	 * Takes the transform set on an actor and copies it to the Data Storage or removes the
	 * transform column if there's not transform available anymore.
	 */
	void RegisterActorLocalTransformToColumn(ITypedElementDataStorageInterface& DataStorage) const;
	/**
	 * Takes the transform stored in the Data Storage and copies it to the actor's tranform if
	 * the FTypedElementSyncBackToWorldTag has been set.
	 */
	void RegisterLocalTransformColumnToActor(ITypedElementDataStorageInterface& DataStorage) const;
};
