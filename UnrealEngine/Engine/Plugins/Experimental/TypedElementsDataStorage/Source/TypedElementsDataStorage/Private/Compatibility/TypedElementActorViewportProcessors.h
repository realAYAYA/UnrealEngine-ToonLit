// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorViewportProcessors.generated.h"

UCLASS()
class UTypedElementActorViewportFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementActorViewportFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterSelectionOutlineColorColumnToActor(ITypedElementDataStorageInterface& DataStorage) const;
};
