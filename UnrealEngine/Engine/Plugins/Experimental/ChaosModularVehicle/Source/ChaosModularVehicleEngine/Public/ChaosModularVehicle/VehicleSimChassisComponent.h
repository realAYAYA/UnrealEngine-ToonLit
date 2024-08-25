// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "VehicleSimBaseComponent.h"
#include "VehicleSimChassisComponent.generated.h"


UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Tick, Replication, Cooking, Activation, LOD))
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimChassisComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimChassisComponent();
	virtual ~UVehicleSimChassisComponent() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float AreaMetresSquared;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float DragCoefficient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float DensityOfMedium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float XAxisMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float YAxisMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float AngularDamping;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Chassis; }

	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};
