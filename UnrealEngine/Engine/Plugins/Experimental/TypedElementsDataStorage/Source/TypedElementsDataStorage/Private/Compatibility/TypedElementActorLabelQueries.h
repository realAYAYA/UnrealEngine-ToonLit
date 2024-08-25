// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorLabelQueries.generated.h"


UCLASS()
class UTypedElementActorLabelFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementActorLabelFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	/**
	 * Takes the label set on an actor and copies it to the Data Storage if they differ.
	 */
	void RegisterActorLabelToColumnQuery(ITypedElementDataStorageInterface& DataStorage) const;
	/**
	 * Takes the label stored in the Data Storage and copies it to the actor's label if the FTypedElementSyncBackToWorldTag
	 * has been set and the labels differ.
	 */
	void RegisterLabelColumnToActorQuery(ITypedElementDataStorageInterface& DataStorage) const;
};