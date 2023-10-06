// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component that is compatible with the navigation system's PathFollowingComponent
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "GameFramework/MovementComponent.h"
#include "NavMovementComponent.generated.h"

class UCapsuleComponent;

/**
 * NavMovementComponent defines base functionality for MovementComponents that move any 'agent' that may be involved in AI pathfinding.
 */
UCLASS(abstract, config=Engine, MinimalAPI)
class UNavMovementComponent : public UMovementComponent
{
	GENERATED_UCLASS_BODY()

	/** Properties that define how the component can move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NavMovement, meta = (DisplayName = "Movement Capabilities", Keywords = "Nav Agent"))
	FNavAgentProperties NavAgentProps;

protected:
	/** Braking distance override used with acceleration driven path following (bUseAccelerationForPaths) */
	UPROPERTY(EditAnywhere, Category = NavMovement, meta = (EditCondition = "bUseFixedBrakingDistanceForPaths"))
	float FixedPathBrakingDistance;

	/** If set to true NavAgentProps' radius and height will be updated with Owner's collision capsule size */
	UPROPERTY(EditAnywhere, Category = NavMovement)
	uint8 bUpdateNavAgentWithOwnersCollision:1;

	/** If set, pathfollowing will control character movement via acceleration values. If false, it will set velocities directly. */
	UPROPERTY(EditAnywhere, Category = NavMovement, GlobalConfig)
	uint8 bUseAccelerationForPaths : 1;

	/** If set, FixedPathBrakingDistance will be used for path following deceleration */
	UPROPERTY(EditAnywhere, Category = NavMovement, meta = (EditCondition = "bUseAccelerationForPaths"))
	uint8 bUseFixedBrakingDistanceForPaths : 1;

	/** If set, StopActiveMovement call will abort current path following request */
	uint8 bStopMovementAbortPaths:1;

public:
	/** Expresses runtime state of character's movement. Put all temporal changes to movement properties here */
	UPROPERTY()
	FMovementProperties MovementState;

private:
	/** object implementing IPathFollowingAgentInterface. Is private to control access to it.
	 *	@see SetPathFollowingAgent, GetPathFollowingAgent */
	UPROPERTY()
	TObjectPtr<UObject> PathFollowingComp;

public:
	/** Stops applying further movement (usually zeros acceleration). */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement")
	ENGINE_API virtual void StopActiveMovement();

	/** Stops movement immediately (reset velocity) but keeps following current path */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	ENGINE_API void StopMovementKeepPathing();

	// Overridden to also call StopActiveMovement().
	ENGINE_API virtual void StopMovementImmediately() override;

	ENGINE_API void SetUpdateNavAgentWithOwnersCollisions(bool bUpdateWithOwner);
	FORCEINLINE bool ShouldUpdateNavAgentWithOwnersCollision() const { return bUpdateNavAgentWithOwnersCollision != 0; }
	
	ENGINE_API void UpdateNavAgent(const AActor& InOwner);
	ENGINE_API void UpdateNavAgent(const UCapsuleComponent& CapsuleComponent);

	/** Returns location of controlled actor - meaning center of collision bounding box */
	FORCEINLINE FVector GetActorLocation() const { return UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector(FLT_MAX); }
	/** Returns location of controlled actor's "feet" meaning center of bottom of collision bounding box */
	virtual FVector GetActorFeetLocation() const { return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - FVector(0,0,UpdatedComponent->Bounds.BoxExtent.Z)) : FNavigationSystem::InvalidLocation; }
	/** Returns based location of controlled actor */
	ENGINE_API virtual FBasedPosition GetActorFeetLocationBased() const;
	/** Returns navigation location of controlled actor */
	FORCEINLINE FVector GetActorNavLocation() const { INavAgentInterface* MyOwner = Cast<INavAgentInterface>(GetOwner()); return MyOwner ? MyOwner->GetNavAgentLocation() : FNavigationSystem::InvalidLocation; }
	/** Returns the full world-coordinates transform of the associated scene component (the UpdatedComponent) */
	FORCEINLINE FTransform GetActorTransform() const { return UpdatedComponent ? UpdatedComponent->GetComponentTransform() : FTransform(); }

	/** path following: request new velocity */
	ENGINE_API virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed);

	/** path following: request new move input (normal vector = full strength) */
	ENGINE_API virtual void RequestPathMove(const FVector& MoveInput);

	/** check if current move target can be reached right now if positions are matching
	 *  (e.g. performing scripted move and can't stop) */
	ENGINE_API virtual bool CanStopPathFollowing() const;

	/** Returns braking distance for acceleration driven path following */
	ENGINE_API virtual float GetPathFollowingBrakingDistance(float MaxSpeed) const;

	void SetPathFollowingAgent(IPathFollowingAgentInterface* InPathFollowingAgent) { PathFollowingComp = Cast<UObject>(InPathFollowingAgent); }
	IPathFollowingAgentInterface* GetPathFollowingAgent() { return Cast<IPathFollowingAgentInterface>(PathFollowingComp); }
	const IPathFollowingAgentInterface* GetPathFollowingAgent() const { return Cast<const IPathFollowingAgentInterface>(PathFollowingComp); }

	/** set fixed braking distance */
	ENGINE_API void SetFixedBrakingDistance(float DistanceToEndOfPath);

	/** clears fixed braking distance */
	ENGINE_API void ClearFixedBrakingDistance();

	FORCEINLINE bool UseAccelerationForPathFollowing() const { return bUseAccelerationForPaths; }

	/** Returns the NavAgentProps(const) */
	FORCEINLINE const FNavAgentProperties& GetNavAgentPropertiesRef() const { return NavAgentProps; }
	/** Returns the NavAgentProps */
	FORCEINLINE FNavAgentProperties& GetNavAgentPropertiesRef() { return NavAgentProps; }

	/** Resets runtime movement state to character's movement capabilities */
	void ResetMoveState() { MovementState = NavAgentProps; }

	/** Returns true if path following can start */
	virtual bool CanStartPathFollowing() const { return true; }

	/** Returns true if component can crouch */
	FORCEINLINE bool CanEverCrouch() const { return NavAgentProps.bCanCrouch; }

	/** Returns true if component can jump */
	FORCEINLINE bool CanEverJump() const { return NavAgentProps.bCanJump; }

	/** Returns true if component can move along the ground (walk, drive, etc) */
	FORCEINLINE bool CanEverMoveOnGround() const { return NavAgentProps.bCanWalk; }

	/** Returns true if component can swim */
	FORCEINLINE bool CanEverSwim() const { return NavAgentProps.bCanSwim; }

	/** Returns true if component can fly */
	FORCEINLINE bool CanEverFly() const { return NavAgentProps.bCanFly; }

	/** Returns true if component is allowed to jump */
	FORCEINLINE bool IsJumpAllowed() const { return CanEverJump() && MovementState.bCanJump; }

	/** Sets whether this component is allowed to jump */
	FORCEINLINE void SetJumpAllowed(bool bAllowed) { MovementState.bCanJump = bAllowed; }

	/** Returns true if currently crouching */ 
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	ENGINE_API virtual bool IsCrouching() const;

	/** Returns true if currently falling (not flying, in a non-fluid volume, and not on the ground) */ 
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	ENGINE_API virtual bool IsFalling() const;

	/** Returns true if currently moving on the ground (e.g. walking or driving) */ 
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	ENGINE_API virtual bool IsMovingOnGround() const;
	
	/** Returns true if currently swimming (moving through a fluid volume) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	ENGINE_API virtual bool IsSwimming() const;

	/** Returns true if currently flying (moving through a non-fluid volume without resting on the ground) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	ENGINE_API virtual bool IsFlying() const;
};


inline bool UNavMovementComponent::IsCrouching() const
{
	return false;
}

inline bool UNavMovementComponent::IsFalling() const
{
	return false;
}

inline bool UNavMovementComponent::IsMovingOnGround() const
{
	return false;
}

inline bool UNavMovementComponent::IsSwimming() const
{
	return false;
}

inline bool UNavMovementComponent::IsFlying() const
{
	return false;
}

inline void UNavMovementComponent::StopMovementKeepPathing()
{
	bStopMovementAbortPaths = false;
	StopMovementImmediately();
	bStopMovementAbortPaths = true;
}

inline void UNavMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	StopActiveMovement();
}
