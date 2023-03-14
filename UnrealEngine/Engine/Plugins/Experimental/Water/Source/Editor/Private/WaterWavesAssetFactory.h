// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "WaterWavesAssetFactory.generated.h"

UCLASS()
class UWaterWavesAssetFactory : public UFactory
{
	GENERATED_BODY()

public:

	UWaterWavesAssetFactory();

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetToolTip() const override;
	// End of UFactory interface
};