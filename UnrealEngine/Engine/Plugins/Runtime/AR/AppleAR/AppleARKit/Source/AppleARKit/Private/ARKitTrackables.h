// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ARTrackable.h"

#include "ARKitTrackables.generated.h"

UCLASS(NotBlueprintType)
class UARKitMeshGeometry : public UARMeshGeometry
{
	GENERATED_BODY()
	
public:
	bool GetObjectClassificationAtLocation(const FVector& InWorldLocation, EARObjectClassification& OutClassification, FVector& OutClassificationLocation, float MaxLocationDiff = 10.f) override;
};
