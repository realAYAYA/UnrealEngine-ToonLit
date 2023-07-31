// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructView.h"
#include "UObject/Interface.h"
#include "GameplayTaskTransition.generated.h"

class UCharacterMovementComponent;
class UGameplayActuationComponent;
class UGameplayTasksComponent;
class UGameplayTask;

/** Transition task completion result */
UENUM()
enum class EGameplayTransitionResult : uint8
{
	Cancelled,
	Succeeded,
	Failed,
};

/** Delegate called when a transition task is completed. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FGameplayTransitionCompleted, const EGameplayTransitionResult /*Result*/, UGameplayTask* /*Task*/);

/** Context containing common data when making and triggering transitions. */
struct FMakeGameplayTransitionTaskContext
{
	/** Current actor */
	AActor* Actor = nullptr;

	/** Task component of the actor */
	UGameplayTasksComponent* TasksComponent = nullptr;

	/** Movement component of the actor */
	UCharacterMovementComponent* MovementComponent = nullptr;

	/** Actuation component of the actor */
	UGameplayActuationComponent* ActuationComponent = nullptr;

	/** Current actuation state. */
	FConstStructView CurrentActuationState;
	
	/** Next actuation state. */
	FConstStructView NextActuationState;
};

/**
 * Interface to handle a GameplayTask as a transition.
 */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UGameplayTaskTransition : public UInterface
{
	GENERATED_BODY()
};

class IGameplayTaskTransition
{
	GENERATED_BODY()

public:

	/** @returns transition completed delegate associated with the transition */
	virtual FGameplayTransitionCompleted& GetTransitionCompleted() = 0;

	/** @return true if the transition should activated. */
	virtual bool ShouldActivate(const FMakeGameplayTransitionTaskContext& Context) const { return true; }
};

/**
 * Describes and creates a transition task.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayTransitionDesc
{
	GENERATED_BODY()

	virtual ~FGameplayTransitionDesc() {}
	
	virtual UGameplayTask* MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const { return nullptr; } 
};
