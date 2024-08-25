// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementHierarchyQueries.generated.h"

/**
 * Calls to queries for general hierarchy management.
 */
UCLASS()
class UTypedElementHiearchyQueriesFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementHiearchyQueriesFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
};
