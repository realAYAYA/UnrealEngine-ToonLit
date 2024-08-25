// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "VehicleSimBaseComponent.h"
#include "VehicleSimThrusterComponent.generated.h"


UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Tick, Replication, Cooking, Activation, LOD))
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimThrusterComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:

	UVehicleSimThrusterComponent();
	virtual ~UVehicleSimThrusterComponent() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MaxThrustForce;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector ForceAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector ForceOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool bSteeringEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector SteeringAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MaxSteeringAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SteeringForceEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float BoostMultiplierEffect;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Thruster; }

	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};
