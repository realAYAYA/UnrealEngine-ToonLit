// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulationFactory.h"
#include "Containers/ArrayView.h"
#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ClothingSimulationFactoryNv.generated.h"

class IClothingSimulation;
class UClothConfigBase;
class UClothingAssetBase;
class UClothingSimulationInteractor;
class UEnum;
class UObject;
template <class TClass> class TSubclassOf;

class UE_DEPRECATED(5.1, "NvCloth simulation is no longer supported, UChaosClothingSimulationFactory should be used going forward.") UClothingSimulationFactoryNv;

UCLASS(MinimalAPI)
class UClothingSimulationFactoryNv final : public UClothingSimulationFactory
{
	GENERATED_BODY()
public:

	CLOTHINGSYSTEMRUNTIMENV_API virtual IClothingSimulation* CreateSimulation() override;
	CLOTHINGSYSTEMRUNTIMENV_API virtual void DestroySimulation(IClothingSimulation* InSimulation) override;
	CLOTHINGSYSTEMRUNTIMENV_API virtual bool SupportsAsset(UClothingAssetBase* InAsset) override;

	CLOTHINGSYSTEMRUNTIMENV_API virtual bool SupportsRuntimeInteraction() override;
	CLOTHINGSYSTEMRUNTIMENV_API virtual UClothingSimulationInteractor* CreateInteractor() override;

	CLOTHINGSYSTEMRUNTIMENV_API virtual TArrayView<const TSubclassOf<UClothConfigBase>> GetClothConfigClasses() const override;
	CLOTHINGSYSTEMRUNTIMENV_API virtual const UEnum* GetWeightMapTargetEnum() const override;
};
