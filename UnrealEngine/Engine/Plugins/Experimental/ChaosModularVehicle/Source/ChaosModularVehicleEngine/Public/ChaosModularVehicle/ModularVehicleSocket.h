// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "ModularVehicleSocket.generated.h"

class UClusterUnionVehicleComponent;

USTRUCT(BlueprintType)
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleSocket
{
	GENERATED_USTRUCT_BODY()

	FModularVehicleSocket();

	/**
	 *	Defines a named attachment location on the Modular vehicle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FVector RelativeLocation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket Parameters")
	FRotator RelativeRotation;

	FVector GetLocation(const class UClusterUnionVehicleComponent* Component) const;

	/** returns FTransform of Socket local transform */
	FTransform GetLocalTransform() const;

	/** Utility that returns the current transform for this socket. */
	FTransform GetTransform(const class UClusterUnionVehicleComponent* Component) const;

};



