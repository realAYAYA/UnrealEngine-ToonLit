// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "VehicleSimBaseComponent.h"
#include "VehicleSimSuspensionComponent.generated.h"


UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Replication, Cooking, Activation, LOD, Physics, Collision, AssetUserData, Event))
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimSuspensionComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:

	UVehicleSimSuspensionComponent();
	virtual ~UVehicleSimSuspensionComponent() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	FVector SuspensionAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SuspensionMaxRaise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SuspensionMaxDrop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SpringRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SpringPreload;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SpringDamping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SuspensionForceEffect;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	//float SwaybarEffect;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Suspension; }
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};
