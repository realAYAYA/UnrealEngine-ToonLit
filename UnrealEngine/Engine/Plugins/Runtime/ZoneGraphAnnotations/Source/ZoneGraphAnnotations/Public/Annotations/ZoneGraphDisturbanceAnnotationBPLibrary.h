// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphDisturbanceAnnotationBPLibrary.generated.h"

/**
 *	Set of utilities for dealing with Disturbance Annotation.
 */
UCLASS()
class ZONEGRAPHANNOTATIONS_API UZoneGraphDisturbanceAnnotationBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/*
	 * Triggers Danger event at specific location.
	 * @param Instigator (optional) identifies this event coming from specific Instigator, only one danger will persist per instigator.
	 * @param Position Position of the danger.
	 * @param Radius Radius of the danger.
	 * @param Duration Duration of the danger.
	 */
	UFUNCTION(BlueprintCallable, Category = "ZoneGraphAnnotations", meta = (WorldContext = "WorldContextObject"))
	static void TriggerDanger(UObject* WorldContextObject, const AActor* Instigator, const FVector Position, const float Radius, const float Duration);
	
};
