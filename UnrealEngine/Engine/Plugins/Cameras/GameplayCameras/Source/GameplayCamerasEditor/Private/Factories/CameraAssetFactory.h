// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "CameraAssetFactory.generated.h"

class UCameraAsset;

/**
 * Implements a factory for UCameraAsset objects.
 */
UCLASS(hidecategories=Object)
class UCameraAssetFactory : public UFactory
{
	GENERATED_BODY()

	UCameraAssetFactory(const FObjectInitializer& ObjectInitializer);

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
};

