// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavigationTypes.h"
#include "StructView.h"
#include "GameplayActuationState.generated.h"


struct FNavCorridor;

/** Base struct for all actuation states */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayActuationStateBase
{
	GENERATED_BODY()

	virtual ~FGameplayActuationStateBase() {}

	/** Called when state is activated. */
	virtual void OnStateActivated(FConstStructView PreviousState) {}
	/** Called when state is deactivated. */
	virtual void OnStateDeactivated(FConstStructView NextState) {}

	/** Name of the actuation the state describes. */	
	UPROPERTY()
	FName ActuationName;
	
	// @todo: MoveTo should use this location.
	/** Nearest navigable location. For movement states that go outside the navigable area, this is the nearest nav location. */
	UPROPERTY()
	FVector NavigationLocation = FVector::ZeroVector;
};

/** Information about a predicted future location */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayActuationPredictedLocation
{
	GENERATED_BODY()

	/** Location in world space */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Heading direction at the location */
	UPROPERTY()
	FVector3f Direction = FVector3f::ForwardVector;

	/** Predicted location time */
	UPROPERTY()
	float Time = 0.0f;

	/** Assumed speed at predicted location. */
	UPROPERTY()
	float Speed = 0.0f;
};

/** Describes movement state. */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayActuationState_Moving : public FGameplayActuationStateBase
{
	GENERATED_BODY()

	/** Heading direction */
	UPROPERTY()
	FVector3f HeadingDirection = FVector3f::ZeroVector;

	/** Predicted future location */
	UPROPERTY()
	FGameplayActuationPredictedLocation Prediction;

	/** Current path. */
	FNavPathSharedPtr Path = nullptr;

	/** Current path corridor. */
	TSharedPtr<FNavCorridor> Corridor = nullptr;
};

/** Describes movement state while standing. */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayActuationState_Standing : public FGameplayActuationState_Moving
{
	GENERATED_BODY()
};

/** Describes movement state during movement transition. */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayActuationState_MovingTransition : public FGameplayActuationState_Moving
{
	GENERATED_BODY()
};
