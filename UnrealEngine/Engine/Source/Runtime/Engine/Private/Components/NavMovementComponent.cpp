// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/NavMovementComponent.h"
#include "AI/NavigationSystemBase.h"
#include "Components/CapsuleComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavMovementComponent)


//----------------------------------------------------------------------//
// UMovementComponent
//----------------------------------------------------------------------//
UNavMovementComponent::UNavMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUpdateNavAgentWithOwnersCollision(true)
	, bUseAccelerationForPaths(false)
	, bUseFixedBrakingDistanceForPaths(false)
	, bStopMovementAbortPaths(true)
{
	bComponentShouldUpdatePhysicsVolume = true;
}

FBasedPosition UNavMovementComponent::GetActorFeetLocationBased() const
{
	return FBasedPosition(NULL, GetActorFeetLocation());
}

void UNavMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	Velocity = MoveVelocity;
}

void UNavMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	// empty in base class, requires at least PawnMovementComponent for input related operations
}

bool UNavMovementComponent::CanStopPathFollowing() const
{
	return true;
}

float UNavMovementComponent::GetPathFollowingBrakingDistance(float MaxSpeed) const
{
	return bUseFixedBrakingDistanceForPaths ? FixedPathBrakingDistance : MaxSpeed;
}

void UNavMovementComponent::SetFixedBrakingDistance(float DistanceToEndOfPath)
{
	if (DistanceToEndOfPath > UE_KINDA_SMALL_NUMBER)
	{
		bUseFixedBrakingDistanceForPaths = true;
		FixedPathBrakingDistance = DistanceToEndOfPath;
	}
}

void UNavMovementComponent::ClearFixedBrakingDistance()
{
	bUseFixedBrakingDistanceForPaths = false;
}

void UNavMovementComponent::StopActiveMovement()
{
	if (!bStopMovementAbortPaths)
	{
		return;
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		PFAgent->OnUnableToMove(*this);
	}
}

void UNavMovementComponent::UpdateNavAgent(const AActor& Owner)
{
	ensure(&Owner == GetOwner());
	if (ShouldUpdateNavAgentWithOwnersCollision() == false)
	{
		return;
	}

	// initialize properties from navigation system
	NavAgentProps.NavWalkingSearchHeightScale = FNavigationSystem::GetDefaultSupportedAgent().NavWalkingSearchHeightScale;
	
	// Can't call GetSimpleCollisionCylinder(), because no components will be registered.
	float BoundRadius, BoundHalfHeight;	
	Owner.GetSimpleCollisionCylinder(BoundRadius, BoundHalfHeight);
	NavAgentProps.AgentRadius = BoundRadius;
	NavAgentProps.AgentHeight = BoundHalfHeight * 2.f;
}

void UNavMovementComponent::UpdateNavAgent(const UCapsuleComponent& CapsuleComponent)
{
	if (ShouldUpdateNavAgentWithOwnersCollision() == false)
	{
		return;
	}

	// initialize properties from navigation system
	NavAgentProps.NavWalkingSearchHeightScale = FNavigationSystem::GetDefaultSupportedAgent().NavWalkingSearchHeightScale;

	NavAgentProps.AgentRadius = CapsuleComponent.GetScaledCapsuleRadius();
	NavAgentProps.AgentHeight = CapsuleComponent.GetScaledCapsuleHalfHeight() * 2.f;
}

void UNavMovementComponent::SetUpdateNavAgentWithOwnersCollisions(bool bUpdateWithOwner)
{
	bUpdateNavAgentWithOwnersCollision = bUpdateWithOwner;
}


