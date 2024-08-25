// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "VehicleSimBaseComponent.h"
#include "VehicleSimClutchComponent.generated.h"


UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Tick, Replication, Cooking, Activation, LOD))
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimClutchComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimClutchComponent();
	virtual ~UVehicleSimClutchComponent() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
		float ClutchStrength;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Clutch; }

	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;

};