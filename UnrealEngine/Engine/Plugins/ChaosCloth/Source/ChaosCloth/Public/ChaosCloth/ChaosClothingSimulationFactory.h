// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulationFactory.h"

#include "ChaosClothingSimulationFactory.generated.h"

UCLASS(MinimalAPI)
class UChaosClothingSimulationFactory final : public UClothingSimulationFactory
{
	GENERATED_BODY()
public:
	CHAOSCLOTH_API virtual IClothingSimulation* CreateSimulation() override;
	CHAOSCLOTH_API virtual void DestroySimulation(IClothingSimulation* InSimulation) override;
	CHAOSCLOTH_API virtual bool SupportsAsset(UClothingAssetBase* InAsset) override;

	CHAOSCLOTH_API virtual bool SupportsRuntimeInteraction() override;
	CHAOSCLOTH_API virtual UClothingSimulationInteractor* CreateInteractor() override;

	CHAOSCLOTH_API virtual TArrayView<const TSubclassOf<UClothConfigBase>> GetClothConfigClasses() const override;
	CHAOSCLOTH_API const UEnum* GetWeightMapTargetEnum() const override;
};
