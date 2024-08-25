// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/CharacterMoverComponent.h"

#include "DefaultMovementSet/Modes/FallingMode.h"
#include "DefaultMovementSet/Modes/FlyingMode.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "MoveLibrary/FloorQueryUtils.h"

UCharacterMoverComponent::UCharacterMoverComponent()
{
	// Default movement modes
	MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UWalkingMode>(TEXT("DefaultWalkingMode")));
	MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UFallingMode>(TEXT("DefaultFallingMode")));
	MovementModes.Add(DefaultModeNames::Flying,  CreateDefaultSubobject<UFlyingMode>(TEXT("DefaultFlyingMode")));

	StartingMovementMode = DefaultModeNames::Falling;
}

bool UCharacterMoverComponent::IsFalling() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Falling;
	}

	return false;
}

bool UCharacterMoverComponent::IsAirborne() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Flying || CachedLastSyncState.MovementMode == DefaultModeNames::Falling;
	}

	return false;
}

bool UCharacterMoverComponent::IsOnGround() const
{
	if (bHasValidCachedState)
	{
		return CachedLastSyncState.MovementMode == DefaultModeNames::Walking;
	}

	return false;
}

bool UCharacterMoverComponent::IsSlopeSliding() const
{
	if (IsAirborne())
	{
		FFloorCheckResult HitResult;
		const UMoverBlackboard* MoverBlackboard = GetSimBlackboard();
		if (MoverBlackboard->TryGet(CommonBlackboard::LastFloorResult, HitResult))
		{
			return HitResult.bBlockingHit && !HitResult.bWalkableFloor;
		}
	}

	return false;
}
