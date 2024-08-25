// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ActorModifierCoreBase.h"
#include "ActorModifierCoreDefs.h"
#include "ActorModifierCoreStack.generated.h"

class USceneComponent;

/** A modifier stack contains modifiers and is also a modifier by itself */
UCLASS(MinimalAPI, BlueprintType, DefaultToInstanced, EditInlineNew)
class UActorModifierCoreStack : public UActorModifierCoreBase
{
	GENERATED_BODY()

	friend class UActorModifierCoreBase;
	friend class UActorModifierCoreSubsystem;

	friend class FActorModifierCoreEditorDetailCustomization;
	friend class UActorModifierCoreEditorStackCustomization;

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnModifierUpdated, UActorModifierCoreBase* /** UpdatedItem */)

	/** Called when a modifier is added to the stack */
	ACTORMODIFIERCORE_API static FOnModifierUpdated OnModifierAddedDelegate;

	/** Called when a modifier is removed from the stack */
	ACTORMODIFIERCORE_API static FOnModifierUpdated OnModifierRemovedDelegate;

	/** Called when a modifier is moved in the stack */
	ACTORMODIFIERCORE_API static FOnModifierUpdated OnModifierMovedDelegate;

	/** Create a new stack by passing the actor and the parent stack if there is one */
	static UActorModifierCoreStack* Create(AActor* InActor, UActorModifierCoreStack* InParentStack = nullptr);

	/** Gets all modifiers in this stack, does not recurse */
	TConstArrayView<UActorModifierCoreBase*> GetModifiers() const
	{
		return Modifiers;
	}

	/** Get modifiers of a specific class only in this stack, does not recurse */
	template <class InModifierType
		UE_REQUIRES(std::is_base_of<UActorModifierCoreBase, InModifierType>::value)>
	void GetClassModifiers(TArray<InModifierType*>& OutModifiers) const
	{
		for (const TObjectPtr<UActorModifierCoreBase>& Modifier : Modifiers)
		{
			if (Modifier->IsA(InModifierType::StaticClass()))
			{
				OutModifiers.Add(static_cast<InModifierType*>(Modifier));
			}
		}
	}

	/** Gets the first modifier in this stack, does not recurse */
	ACTORMODIFIERCORE_API UActorModifierCoreBase* GetFirstModifier() const;

	/** Gets the last modifier in this stack, does not recurse */
	ACTORMODIFIERCORE_API UActorModifierCoreBase* GetLastModifier() const;

	/** Check that we have a modifier inside this stack, checks also nested stacks */
	ACTORMODIFIERCORE_API bool ContainsModifier(const FName& InSearchName, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;
	ACTORMODIFIERCORE_API bool ContainsModifier(const UClass* InSearchClass, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;
	ACTORMODIFIERCORE_API bool ContainsModifier(const UActorModifierCoreBase* InSearchModifier, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;

	/** Finds a modifier inside this stack, returns first found, checks also nested stacks */
	ACTORMODIFIERCORE_API UActorModifierCoreBase* FindModifier(FName InSearchName, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;
	ACTORMODIFIERCORE_API UActorModifierCoreBase* FindModifier(const UClass* InSearchClass, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;

	/** Finds modifiers inside this stack, checks also nested stacks */
	ACTORMODIFIERCORE_API TArray<UActorModifierCoreBase*> FindModifiers(FName InSearchName, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;
	ACTORMODIFIERCORE_API TArray<UActorModifierCoreBase*> FindModifiers(const UClass* InSearchClass, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const;

	/** This is the root actor stack if we do not have any parent stack */
	bool IsRootStack() const
	{
		return !ModifierStack.IsValid();
	}

	/** Execute those function when the stack is restored, before executing it again */
	ACTORMODIFIERCORE_API void ProcessFunctionOnRestore(const TFunction<void()>& InFunction);

	/** Execute those function when the stack is on idle, done with updates */
	void ProcessFunctionOnIdle(const TFunction<void()>& InFunction);

	/** Process a search function to use before/after position context, stops when false is returned */
	ACTORMODIFIERCORE_API bool ProcessSearchFunction(TFunctionRef<bool(const UActorModifierCoreBase*)> InFunction, const FActorModifierCoreStackSearchOp& InSearchOptions) const;

	/** Does this stack contains any modifiers */
	virtual bool IsModifierEmpty() const override
	{
		return Modifiers.IsEmpty();
	}

	/** Set profiling mode for stack and modifiers inside */
	ACTORMODIFIERCORE_API void SetModifierProfiling(bool bInProfiling);

protected:
	//~ Begin UObject
	virtual void PostLoad() override;
	//~ End UObject

	/** Process a function through each modifier in the stack and also the stacks below, stop when we return false */
	virtual bool ProcessFunction(TFunctionRef<bool(const UActorModifierCoreBase*)> InFunction, const FActorModifierCoreStackSearchOp& InSearchOptions = FActorModifierCoreStackSearchOp::GetDefault()) const override;

	/** Checks that we have a modifier with this name inside this stack before another modifier, checks also nested stacks */
	bool ContainsModifierBefore(const FName& InSearchName, const UActorModifierCoreBase* InBeforeModifier) const;
	bool ContainsModifierBefore(const UActorModifierCoreBase* InSearchModifier, const UActorModifierCoreBase* InBeforeModifier) const;

	/** Checks that we have a modifier with this name inside this stack after another modifier, checks also nested stacks */
	bool ContainsModifierAfter(const FName& InSearchName, const UActorModifierCoreBase* InAfterModifier) const;
	bool ContainsModifierAfter(const UActorModifierCoreBase* InSearchModifier, const UActorModifierCoreBase* InAfterModifier) const;

	/** Checks whether all modifier in this stack are initialized */
	bool IsModifierStackInitialized() const;

	/** Gets all modifiers found after this one in the stack that depends on this modifier */
	bool GetDependentModifiers(UActorModifierCoreBase* InModifier, TSet<UActorModifierCoreBase*>& OutDependentModifiers) const;

	/** Gets all modifiers found before this one in the stack that are required by this modifier */
	bool GetRequiredModifiers(UActorModifierCoreBase* InModifier, TSet<UActorModifierCoreBase*>& OutDependentModifiers) const;

	/** Clone a modifier with options from another stack/actor, returns the newly inserted modifier, supports BATCH operation */
	UActorModifierCoreBase* CloneModifier(FActorModifierCoreStackCloneOp& InCloneOp);

	/** Insert/Add a modifier with options in the stack, returns the newly inserted modifier, supports BATCH operation */
	UActorModifierCoreBase* InsertModifier(FActorModifierCoreStackInsertOp& InInsertOp);

	/** Moves a modifier with options in the stack, if fail will return a failing reason, supports BATCH operation */
	bool MoveModifier(FActorModifierCoreStackMoveOp& InMoveOp);

	/** Removes a modifier from this stack, supports BATCH operation */
	bool RemoveModifier(FActorModifierCoreStackRemoveOp& InRemoveOp);

	/** Removes all modifiers from this stack in one batch to reduce updates */
	bool RemoveAllModifiers();

	/** Register this stack to the subsystem to query it later only if root stack */
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;

	/** Unregister this stack to the subsystem and propagates to the children modifiers */
	virtual void OnModifierRemoved(EActorModifierCoreDisableReason InReason) override;

	/** Called when the whole stack is disabled, propagates to the children modifiers */
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;

	/** Called when the whole stack is enabled again, propagates to the children modifiers */
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;

	/** A stack will run if all its modifiers are ready to run */
	virtual bool IsModifierReady() const override;

	/** Restore the state before this stack was apply by reversing executed modifiers */
	virtual void RestorePreState() override;

	/** Execute this stack and the modifiers it contains */
	virtual void Apply() override;

	/** Calls OnModifyingActorTransformed of each modifier in the stack if enabled */
	virtual void OnModifiedActorTransformed() override;

	/** Called when a modifier in the stack is dirty */
	virtual void OnModifierDirty(UActorModifierCoreBase* DirtyModifier, bool bExecute) override;

	/** Contains actual modifiers in the stack */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, NoClear, Export, Instanced, Category = "Modifiers")
	TArray<TObjectPtr<UActorModifierCoreBase>> Modifiers;

private:
	/** Sets the stack to receive tick events */
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;

	/** Checks if any modifier within the stack can be set dirty */
	virtual bool IsModifierDirtyable() const override;

	/** Unregister this stack to the subsystem when this actor is destroyed */
	UFUNCTION()
	void OnActorDestroyed(AActor* InActor);

	/** Called when the actor transform is updated */
	void OnActorTransformUpdated(USceneComponent*, EUpdateTransformFlags, ETeleportType);

	/** Will execute the chain of modifier in this stack */
	void ApplyChainModifiers();

	/** Builds the execution chain of modifiers */
	void BuildExecutionChain();

	/** Schedules a modifier optimization on idle */
	void ScheduleModifierOptimization(bool bInInvalidateAll = false);

	/** Checks for any possible modifier optimization within the stack */
	void CheckModifierOptimization(bool bInInvalidateAll);

	/** Contains a copy of modifiers in the stack for this round of execution, useful for restore and for query, can be different from modifiers array */
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TArray<TObjectPtr<UActorModifierCoreBase>> CurrentModifiers;

	/** Contains the modifiers that we will execute this round, can be different from modifiers array */
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TArray<TObjectPtr<UActorModifierCoreBase>> ExecuteModifiers;

	/** Functions to execute once when the stack is on idle */
	TArray<TFunction<void()>> OnIdleFunctions;

	/** Functions to execute once when the stack is restored */
	TArray<TFunction<void()>> OnRestoreFunctions;

	/** Enable profiling for the modifiers in this stack */
	UPROPERTY(EditInstanceOnly, Category="Modifier", meta=(DisplayName="Enable Profiling"))
	bool bModifierProfiling = false;

	int32 ModifierExecutionIdx = INDEX_NONE;

	bool bAllModifiersDirty = true;
};
