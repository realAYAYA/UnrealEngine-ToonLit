// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Factories/Factory.h"
#include "PlacementPaletteItem.h"

#include "PlacementPaletteAsset.generated.h"

class UPlacementPaletteAsset;

UCLASS(NotPlaceable)
class UPlacementPaletteAsset : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Items)
	TArray<TObjectPtr<UPlacementPaletteClient>> PaletteItems;

	UPROPERTY()
	FGuid GridGuid;
};

UCLASS(hideCategories=Object)
class UPlacementPaletteAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
	virtual FText GetToolTip() const override;
};
