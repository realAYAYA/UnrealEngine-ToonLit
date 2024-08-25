// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "ModularVehicleClusterPawn.generated.h"

class UClusterUnionVehicleComponent;
class UModularVehicleBaseComponent;

UCLASS()
class CHAOSMODULARVEHICLEENGINE_API AModularVehicleClusterPawn: public APawn
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION()
	UClusterUnionVehicleComponent* GetClusterUnionComponent() const { return ClusterUnionVehicleComponent; }

	UFUNCTION()
	UModularVehicleBaseComponent* GetVehicleSimulationComponent() const { return VehicleSimComponent; }

	/* VehicleSpecificClusterComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vehicle, meta = (ExposeFunctionCategories = "Components|ModularVehicle", AllowPrivateAccess = "true"))
	TObjectPtr<UClusterUnionVehicleComponent> ClusterUnionVehicleComponent;

	/* ModularVehicleComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vehicle, meta = (ExposeFunctionCategories = "Components|ModularVehicle", AllowPrivateAccess = "true"))
	TObjectPtr<UModularVehicleBaseComponent> VehicleSimComponent;

};
