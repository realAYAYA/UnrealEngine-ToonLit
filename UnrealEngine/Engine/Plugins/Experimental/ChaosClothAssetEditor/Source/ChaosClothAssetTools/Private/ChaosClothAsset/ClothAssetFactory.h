// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"

#include "ClothAssetFactory.generated.h"

/**
 * Having a cloth factory allows the cloth asset to be created from the Editor's menus.
 */
UCLASS(Experimental)
class UChaosClothAssetFactory : public UFactory
{
	GENERATED_BODY()
public:
	UChaosClothAssetFactory(const FObjectInitializer& ObjectInitializer);

	/** UFactory Interface */
	virtual bool CanCreateNew() const override { return true; }
	virtual bool FactoryCanImport(const FString& Filename) override { return false; }
	virtual bool ShouldShowInNewMenu() const override { return true; }
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	/** End UFactory Interface */
};
