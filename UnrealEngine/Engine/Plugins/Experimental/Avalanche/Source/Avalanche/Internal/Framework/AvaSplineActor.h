// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AvaSplineActor.generated.h"

class USplineComponent;

/**
 * Motion Design Spline Actor
 */
UCLASS(MinimalAPI, DisplayName = "Motion Design Spline Actor")
class AAvaSplineActor : public AActor
{
	GENERATED_BODY()

public:
	AAvaSplineActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Design")
	TObjectPtr<USplineComponent> SplineComponent;
};
