// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "Components/PrimitiveComponent.h"

#include "MovementUtils.generated.h"

struct FMovementRecord;
class UMoverComponent;

namespace UE::MoverUtils
{
	extern const double SMALL_MOVE_DISTANCE;
}

/** Encapsulates detailed trajectory sample info, from a move that has already occurred or one projected into the future */
USTRUCT(BlueprintType)
struct MOVER_API FTrajectorySampleInfo
{
	GENERATED_USTRUCT_BODY()

	FTransform Transform;					// Position and orientation (world space)

	FVector LinearVelocity;					// Velocity at the time of this sample (world space, units/sec)
	FVector InstantaneousAcceleration;		// Acceleration at the time of this sample (world space, units/sec^2)

	FRotator AngularVelocity;				// Rotational velocity (world space, degrees/sec)

	float SimTimeMs;						// Time stamp of this sample, in server simulation time
};

// Input parameters for compute velocity function
USTRUCT(BlueprintType)
struct MOVER_API FComputeVelocityParams
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float DeltaSeconds = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector InitialVelocity = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector MoveDirectionIntent = FVector::ZeroVector;

	// AuxState variables
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float MaxSpeed = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float TurningBoost = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Friction = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Deceleration = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Acceleration = 0.f;
};

// Input parameters for ComputeCombinedVelocity()
USTRUCT(BlueprintType)
struct MOVER_API FComputeCombinedVelocityParams
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float DeltaSeconds = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector InitialVelocity = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector MoveDirectionIntent = FVector::ZeroVector;

	// AuxState variables
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float MaxSpeed = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float TurningBoost = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Friction = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Deceleration = 0.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Acceleration = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector ExternalAcceleration = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float OverallMaxSpeed = 0.f;
};

/**
 * MovementUtils: a collection of stateless static BP-accessible functions for a variety of movement-related operations
 */
UCLASS()
class MOVER_API UMovementUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// JAH TODO: Ideally, none of these functions should deal with simulation input/state types. Rework them to take only the core types they actually need
	// JAH TODO: Make sure all 'out' parameters are last in the param list and marked as "Out"
	// JAH TODO: separate out the public-facing ones from the internally-used ones and make all public-facing ones BlueprintCallable

	/** Checks whether a given velocity is exceeding a maximum speed, with some leeway to account for numeric imprecision */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static bool IsExceedingMaxSpeed(const FVector& Velocity, float InMaxSpeed);

	/** Returns new ground-based velocity (worldspace) based on previous state, movement intent (worldspace), and movement settings */
	UFUNCTION(BlueprintCallable, Category=Mover)
    static FVector ComputeVelocity(const FComputeVelocityParams& InParams);

	/** Returns new velocity based on previous state, movement intent, movement mode's influence and movement settings */
	UFUNCTION(BlueprintCallable, Category=Mover)
    static FVector ComputeCombinedVelocity(const FComputeCombinedVelocityParams& InParams);

	/** Returns velocity (units per second) contributed by gravitational acceleration over a given time */
	UFUNCTION(BlueprintCallable, Category=Mover)
	static FVector ComputeVelocityFromGravity(const FVector& GravityAccel, float DeltaSeconds) { return GravityAccel * DeltaSeconds; }

	/** Checks whether a given velocity is strong enough to lift off against gravity */
	UFUNCTION(BlueprintCallable, Category=Mover)
	static bool CanEscapeGravity(const FVector& PriorVelocity, const FVector& NewVelocity, const FVector& GravityAccel, float DeltaSeconds);

	/** Ensures input Vector (typically a velocity, acceleration, or move delta) is limited to a movement plane. 
	* @param bMaintainMagnitude - if true, vector will be scaled after projection in an attempt to keep magnitude the same 
	*/
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector ConstrainToPlane(const FVector& Vector, const FPlane& MovementPlane, bool bMaintainMagnitude=true);

	// Surface sliding

	/** Returns an alternative move delta to slide along a surface, based on parameters describing a blocked attempted move */
	UFUNCTION(BlueprintCallable, Category=Mover)
	static FVector ComputeSlideDelta(const FVector& Delta, const float PctOfDeltaToMove, const FVector& Normal, const FHitResult& Hit);

	/** Returns an alternative move delta when we are in contact with 2 surfaces */
	static FVector ComputeTwoWallAdjustedDelta(const FVector& MoveDelta, const FHitResult& Hit, const FVector& OldHitNormal);

	/** Attempts to move a component along a surface. Returns the percent of time applied, with 0.0 meaning no movement occurred. */
	UFUNCTION(BlueprintCallable, Category=Mover)
	static float TryMoveToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, FMovementRecord& MoveRecord);
	// Component movement

	/** Attempts to move a component and resolve any penetration issues with the proposed move Delta */
	UFUNCTION(BlueprintCallable, Category=Mover)
	static bool TrySafeMoveUpdatedComponent(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport, FMovementRecord& MoveRecord);

	/** Returns a movement step that should get the subject of the hit result out of an initial penetration condition */
	static FVector ComputePenetrationAdjustment(const FHitResult& Hit);
	
	/** Attempts to move out of a situation where the component is stuck in geometry, using a suggested adjustment to start. */
	static bool TryMoveToResolvePenetration(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, EMoveComponentFlags MoveComponentFlags, const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotationQuat, FMovementRecord& MoveRecord);
	static void InitCollisionParams(const UPrimitiveComponent* UpdatedPrimitive, FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParam);
	static bool OverlapTest(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape, const AActor* IgnoreActor);

	/** Computes velocity based on start and end positions over time */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector ComputeVelocityFromPositions(const FVector& FromPos, const FVector& ToPos, float DeltaSeconds);

	/** Computes the angular velocity needed to change from one orientation to another within a time frame. Use the optional TurningRateLimit to clamp to a maximum step (negative=unlimited). */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FRotator ComputeAngularVelocity(const FRotator& From, const FRotator& To, float DeltaSeconds, float TurningRateLimit=-1.f);

	/** Computes the directional movement intent based on input vector and associated type */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector ComputeDirectionIntent(const FVector& MoveInput, EMoveInputType MoveInputType);

	// Internal functions - not meant to be called outside of this library
	
	/** Internal function that other move functions use to perform all actual component movement and retrieve results
	 *  Note: This function moves the character directly and should only be used if needed. Consider using something like TrySafeMoveUpdatedComponent.
	 */
	static bool TryMoveUpdatedComponent_Internal(USceneComponent* UpdatedComponent, const FVector& Delta, const FQuat& NewRotation, bool bSweep, EMoveComponentFlags MoveComponentFlags, FHitResult* OutHit, ETeleportType Teleport);
};
