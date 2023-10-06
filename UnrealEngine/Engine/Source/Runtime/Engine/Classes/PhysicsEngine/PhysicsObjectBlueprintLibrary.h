// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "Chaos/PhysicsObjectInterface.h"
#include "Math/MathFwd.h"

#include "PhysicsObjectBlueprintLibrary.generated.h"

UCLASS()
class UPhysicsObjectBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Physics Object")
	static FClosestPhysicsObjectResult GetClosestPhysicsObjectFromWorldLocation(UPrimitiveComponent* Component, const FVector& WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Physics Object")
	static bool ExtractClosestPhysicsObjectResults(const FClosestPhysicsObjectResult& Result, FName& OutName);

	UFUNCTION(BlueprintPure, Category = "Physics Object")
	static FTransform GetPhysicsObjectWorldTransform(UPrimitiveComponent* Component, FName BoneName);

	UFUNCTION(BlueprintCallable, Category = "Physics Object")
	static void ApplyRadialImpulse(UPrimitiveComponent* Component, FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bApplyStrain);
};