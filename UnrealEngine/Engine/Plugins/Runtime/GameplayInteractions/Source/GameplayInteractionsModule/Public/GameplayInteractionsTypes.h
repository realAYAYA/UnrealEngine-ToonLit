// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "SmartObjectTypes.h"
#include "GameplayInteractionsTypes.generated.h"

struct FNavCorridor;
struct FNavigationPath;

GAMEPLAYINTERACTIONSMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogGameplayInteractions, Warning, All);

namespace UE::GameplayInteraction::Names
{
	const FName ContextActor = TEXT("Actor");					// The actor performing the interaction, using short name to be consistent with naming with StateTreeComponentSchema.
	const FName SmartObjectActor = TEXT("SmartObjectActor");	// The SmartObjectActor participating in the interaction.
	const FName SmartObjectClaimedHandle = TEXT("SmartObjectClaimedHandle");
	const FName SlotEntranceHandle = TEXT("SlotEntranceHandle");
	const FName AbortContext = TEXT("AbortContext");
};

/** Reason why the interaction is ended prematurely. */
UENUM(BlueprintType)
enum class EGameplayInteractionAbortReason : uint8
{
	Unset,
	ExternalAbort,
	InternalAbort,	// Internal failure from slot invalidation (e.g. slot unregistered, destroyed)
};

/**
 * Struct holding data related to the abort action  
 */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayInteractionAbortContext
{
	GENERATED_BODY()

	FGameplayInteractionAbortContext() = default;
	explicit FGameplayInteractionAbortContext(const EGameplayInteractionAbortReason& InReason) : Reason(InReason) {}
	
	UPROPERTY()
	EGameplayInteractionAbortReason Reason = EGameplayInteractionAbortReason::Unset;
};

/**
 * Data added to a Smart Object slot when interaction is started on it. Allows to look up the user.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayInteractionSlotUserData : public FSmartObjectSlotStateData
{
	GENERATED_BODY()

	FGameplayInteractionSlotUserData() = default;
	explicit FGameplayInteractionSlotUserData(AActor* InUserActor) : UserActor(InUserActor) {}
		
	TWeakObjectPtr<AActor> UserActor = nullptr;
};



/**
 * Base class (namespace) for all StateTree Tasks related to AI/Gameplay.
 * This allows schemas to safely include all tasks child of this struct. 
 */
USTRUCT(meta = (Hidden))
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayInteractionStateTreeTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

};

USTRUCT(meta = (Hidden))
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayInteractionStateTreeCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

};

/**
 * Value specifying how GameplayTag container should be modified.
 */
UENUM()
enum class EGameplayInteractionModifyGameplayTagOperation : uint8
{
	/** The tag modified during the lifetime of the task. */
	Add,

	/** The tag modified when the state becomes active. */
	Remove,
};

/**
 * Value specifying when a State Tree task based modification should take place.
 */
UENUM()
enum class EGameplayInteractionTaskModify : uint8
{
	/** The modification takes effect when the state becomes active, and is undone when the state becomes inactive. */
	OnEnterStateUndoOnExitState,

	/** The modification takes place when the state becomes active. */
	OnEnterState,

	/** The modification takes place when the state becomes inactive. */
	OnExitState,

	/** The modification takes place if the state fails. */
	OnExitStateFailed,

	/** The modification takes place if the state succeeds. */
	OnExitStateSucceeded,
};

/**
 * Value specifying when a State Tree task based action should be triggered.
 */
UENUM()
enum class EGameplayInteractionTaskTrigger : uint8
{
	/** Execute when state becomes active. */
	OnEnterState,
	
	/** Execute when state becomes inactive. */
	OnExitState,

	/** Execute if the state fails. */
	OnExitStateFailed,

	/** Execute if the state succeeds. */
	OnExitStateSucceeded,
};


UENUM()
enum class EGameplayTaskActuationResult : uint8
{
	None,
	RequestFailed,
	Failed,
	Succeeded,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGameplayTaskActuationCompleted, EGameplayTaskActuationResult, Result, AActor*, Actor);

namespace UE::GameplayInteraction::Debug
{
	void VLogPath(const UObject* LogOwner, const FNavigationPath& Path);
	void VLogCorridor(const UObject* LogOwner, const FNavCorridor& Corridor);
}; // UE::GameplayInteraction::Debug
