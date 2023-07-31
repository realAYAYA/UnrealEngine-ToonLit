// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulationFactory.h"

#include "ChaosClothingSimulationFactory.generated.h"

UCLASS()
class CHAOSCLOTH_API UChaosClothingSimulationFactory final : public UClothingSimulationFactory
{
	GENERATED_BODY()
public:
	virtual IClothingSimulation* CreateSimulation() override;
	virtual void DestroySimulation(IClothingSimulation* InSimulation) override;
	virtual bool SupportsAsset(UClothingAssetBase* InAsset) override;

	virtual bool SupportsRuntimeInteraction() override;
	virtual UClothingSimulationInteractor* CreateInteractor() override;

	virtual TArrayView<const TSubclassOf<UClothConfigBase>> GetClothConfigClasses() const override;
	const UEnum* GetWeightMapTargetEnum() const override;
};
