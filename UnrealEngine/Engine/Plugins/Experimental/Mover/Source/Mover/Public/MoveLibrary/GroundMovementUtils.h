// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverDataModelTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GroundMovementUtils.generated.h"

class UMoverComponent;
struct FMovementRecord;
struct FFloorCheckResult;
struct FOptionalFloorCheckResult;
struct FHitResult;
struct FProposedMove;

// Input parameters for controlled ground movement function
USTRUCT(BlueprintType)
struct MOVER_API FGroundMoveParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	EMoveInputType MoveInputType = EMoveInputType::DirectionalIntent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector MoveInput = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FRotator OrientationIntent = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector PriorVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FRotator PriorOrientation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float MaxSpeed = 800.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Acceleration = 4000.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Deceleration = 8000.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Friction = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float TurningRate = 500.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float TurningBoost = 8.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector GroundNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float DeltaSeconds = 0.f;
};

/**
 * GroundMovementUtils: a collection of stateless static BP-accessible functions for a variety of ground movement-related operations
 */
UCLASS()
class MOVER_API UGroundMovementUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/** Generate a new movement based on move/orientation intents and the prior state, constrained to the ground movement plane. Also applies deceleration friction as necessary. */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FProposedMove ComputeControlledGroundMove(const FGroundMoveParams& InParams);
	
	// TODO: Refactor this API for fewer parameters
	/** Move up steps or slope. Does nothing and returns false if hit surface is invalid for step-up use */
	static bool TryMoveToStepUp(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& GravDir, float MaxStepHeight, float MaxWalkSlopeCosine, float FloorSweepDistance, const FVector& MoveDelta, const FHitResult& Hit, const FFloorCheckResult& CurrentFloor, bool bIsFalling, FOptionalFloorCheckResult* OutStepDownResult, FMovementRecord& MoveRecord);

	/** Moves vertically to stay within range of the walkable floor. Does nothing and returns false if floor is unwalkable or if already in range. */ 
	static bool TryMoveToAdjustHeightAboveFloor(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, FFloorCheckResult& CurrentFloor, float MaxWalkSlopeCosine, FMovementRecord& MoveRecord);

	/** Attempts to move a component along a surface in the walking mode. Returns the percent of time applied, with 0.0 meaning no movement occurred.
     *  Note: This modifies the normal and calls UMovementUtils::TryMoveToSlideAlongSurface
     */
    UFUNCTION(BlueprintCallable, Category=Mover)
    static float TryWalkToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, FMovementRecord& MoveRecord, float MaxWalkSlopeCosine, float MaxStepHeight);

	/** Used to change a movement to be along a ramp's surface, typically to prevent slow movement when running up/down a ramp */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector ComputeDeflectedMoveOntoRamp(const FVector& OrigMoveDelta, const FHitResult& RampHitResult, float MaxWalkSlopeCosine, const bool bHitFromLineTrace);

	UFUNCTION(BlueprintCallable, Category = Mover)
	static bool CanStepUpOnHitSurface(const FHitResult& Hit);
};
