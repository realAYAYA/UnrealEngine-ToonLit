// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "SmartObjectRuntime.h"
#include "StateTreeExecutionContext.h"
#include "GameplayInteractionContext.generated.h"

class UGameplayInteractionSmartObjectBehaviorDefinition;

/**
 * Struct that holds data required to perform the interaction
 * and wraps StateTree execution
 */
USTRUCT()
struct FGameplayInteractionContext
{
	GENERATED_BODY()

public:
	void SetClaimedHandle(const FSmartObjectClaimHandle& InClaimedHandle) { ClaimedHandle = InClaimedHandle; }
	
	UE_DEPRECATED(5.1, "Please use SetContextActor")
	void SetInteractorActor(AActor* InInteractorActor) { ContextActor = InInteractorActor; }
	UE_DEPRECATED(5.1, "Please use SetSmartObjectActor")
	void SetInteractableActor(AActor* InInteractableActor) { SmartObjectActor = InInteractableActor; }
	
	void SetContextActor(AActor* InContextActor) { ContextActor = InContextActor; }
	void SetSmartObjectActor(AActor* InSmartObjectActor) { SmartObjectActor = InSmartObjectActor; }

	void SetAbortContext(const FGameplayInteractionAbortContext& InAbortContext) { AbortContext = InAbortContext; }

	bool IsValid() const { return ClaimedHandle.IsValid() && ContextActor != nullptr && SmartObjectActor != nullptr; }

	/**
	 * Prepares the StateTree execution context using provided Definition then starts the underlying StateTree 
	 * @return True if interaction has been properly initialized and ready to be ticked.
	 */
	GAMEPLAYINTERACTIONSMODULE_API bool Activate(const UGameplayInteractionSmartObjectBehaviorDefinition& Definition);

	/**
	 * Updates the underlying StateTree
	 * @return True if still requires to be ticked, false if done.
	 */
	GAMEPLAYINTERACTIONSMODULE_API bool Tick(const float DeltaTime);
	
	/**
	 * Stops the underlying StateTree
	 */
	GAMEPLAYINTERACTIONSMODULE_API void Deactivate();
	
	/** Sends event for the StateTree. Will be received on the next tick by the StateTree. */
	GAMEPLAYINTERACTIONSMODULE_API void SendEvent(const FStateTreeEvent& Event);
	
protected:	
	/**
	 * Updates all external data views from the provided interaction context.  
	 * @return True if all external data views are valid, false otherwise.
	 */
	bool SetContextRequirements(FStateTreeExecutionContext& StateTreeContext) const;

	/** @return true of the ContextActor and SmartObjectActor match the ones set in schema. */
	bool ValidateSchema(const FStateTreeExecutionContext& StateTreeContext) const;
	
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;
    
    UPROPERTY()
    FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY()
	FGameplayInteractionAbortContext AbortContext;

	UPROPERTY()
    TObjectPtr<AActor> ContextActor = nullptr;
    
    UPROPERTY()
    TObjectPtr<AActor> SmartObjectActor = nullptr;

	UPROPERTY()
	TObjectPtr<const UGameplayInteractionSmartObjectBehaviorDefinition> Definition = nullptr;
};
