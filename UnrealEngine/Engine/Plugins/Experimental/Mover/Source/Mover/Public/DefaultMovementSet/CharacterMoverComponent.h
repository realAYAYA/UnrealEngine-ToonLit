// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverComponent.h"
#include "CharacterMoverComponent.generated.h"

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class MOVER_API UCharacterMoverComponent : public UMoverComponent
{
	GENERATED_BODY()
	
public:
	UCharacterMoverComponent();
	
	// Is this actor in a falling state? Note that this includes upwards motion induced by jumping.
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsFalling() const;

	// Is this actor in a airborne state? (e.g. Flying, Falling)
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsAirborne() const;

	// Is this actor in a grounded state? (e.g. Walking)
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsOnGround() const;

	// Is this actor sliding on an unwalkable slope
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsSlopeSliding() const;
};
