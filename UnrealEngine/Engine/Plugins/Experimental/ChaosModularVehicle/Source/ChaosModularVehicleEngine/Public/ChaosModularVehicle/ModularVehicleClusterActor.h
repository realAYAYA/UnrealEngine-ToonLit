// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "ModularVehicleClusterActor.generated.h"

class UClusterUnionVehicleComponent;
class UModularVehicleBaseComponent;

UCLASS()
class CHAOSMODULARVEHICLEENGINE_API AModularVehicleClusterActor: public AActor
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
