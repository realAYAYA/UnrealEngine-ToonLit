// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeExecutionTypes.h"
#include "StateTreeNodeBlueprintBase.generated.h"

struct FStateTreeEvent;
struct FStateTreeEventQueue;
struct FStateTreeInstanceStorage;
struct FStateTreeLinker;
struct FStateTreeExecutionContext;

UENUM()
enum class EStateTreeBlueprintPropertyCategory : uint8
{
	NotSet,
	Input,	
	Parameter,
	Output,
	ContextObject,
};


/** Struct use to copy external data to the Blueprint item instance, resolved during StateTree linking. */
struct STATETREEMODULE_API FStateTreeBlueprintExternalDataHandle
{
	const FProperty* Property = nullptr;
	FStateTreeExternalDataHandle Handle;
};


UCLASS(Abstract)
class STATETREEMODULE_API UStateTreeNodeBlueprintBase : public UObject
{
	GENERATED_BODY()

public:
	/** Sends event to the StateTree. */
	UFUNCTION(BlueprintCallable, Category = "StateTree", meta = (HideSelfPin = "true", DisplayName = "StateTree Send Event"))
	void SendEvent(const FStateTreeEvent& Event);

	/** Request state transition. */
	UFUNCTION(BlueprintCallable, Category = "StateTree", meta = (HideSelfPin = "true", DisplayName = "StateTree Request Transition"))
	void RequestTransition(const FStateTreeStateLink& TargetState, const EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal);

protected:
	virtual UWorld* GetWorld() const override;
	AActor* GetOwnerActor(const FStateTreeExecutionContext& Context) const;

	/** These methods are const as they set mutable variables and need to be called from a const method. */
	void SetCachedInstanceDataFromContext(const FStateTreeExecutionContext& Context) const;
	void ClearCachedInstanceData() const;
	
	UE_DEPRECATED(5.2, "Use SetCachedInstanceDataFromContext() instead.")
	void SetCachedEventQueueFromContext(const FStateTreeExecutionContext& Context) const { SetCachedInstanceDataFromContext(Context); }
	UE_DEPRECATED(5.2, "Use ClearCachedInstanceData() instead.")
	void ClearCachedEventQueue() const { ClearCachedInstanceData(); }
	
private:

	/** Cached State where the node is processed on. */
	mutable FStateTreeStateHandle CachedState;
	
	/** Cached instance data while the node is active. */
	mutable FStateTreeInstanceStorage* InstanceStorage = nullptr;

	/** Cached owner while the node is active. */
	UPROPERTY()
	mutable TObjectPtr<UObject> CachedOwner = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "StateTreeEvents.h"
#include "StateTreeExecutionContext.h"
#endif
