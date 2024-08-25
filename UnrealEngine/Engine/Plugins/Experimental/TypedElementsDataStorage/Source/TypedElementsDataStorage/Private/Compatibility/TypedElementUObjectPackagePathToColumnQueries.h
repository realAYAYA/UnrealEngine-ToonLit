// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementUObjectPackagePathToColumnQueries.generated.h"

UCLASS()
class UTypedElementUObjectPackagePathFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementUObjectPackagePathFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterTryAddPackageRef(ITypedElementDataStorageInterface& DataStorage);
	TypedElementDataStorage::QueryHandle TryAddPackageRef = TypedElementDataStorage::InvalidQueryHandle;
};
