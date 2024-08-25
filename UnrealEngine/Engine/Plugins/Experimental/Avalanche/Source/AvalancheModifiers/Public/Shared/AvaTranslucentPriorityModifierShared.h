// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "Containers/Set.h"
#include "Modifiers/ActorModifierCoreSharedObject.h"
#include "Modifiers/AvaTranslucentPriorityModifier.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "AvaTranslucentPriorityModifierShared.generated.h"

class AActor;
class ACameraActor;

USTRUCT()
struct FAvaTranslucentPriorityModifierComponentState
{
	GENERATED_BODY()

	FAvaTranslucentPriorityModifierComponentState() {}
	explicit FAvaTranslucentPriorityModifierComponentState(UPrimitiveComponent* InComponent)
		: PrimitiveComponentWeak(InComponent)
	{}

	/** Save this component state if valid */
	void Save();

	/** Restore this component state if valid */
	void Restore() const;

	/** Get component owning actor */
	AActor* GetOwningActor() const;

	/** Get component world location */
	FVector GetComponentLocation() const;

	friend uint32 GetTypeHash(const FAvaTranslucentPriorityModifierComponentState& InItem)
	{
		return GetTypeHash(InItem.PrimitiveComponentWeak);
	}

	bool operator==(const FAvaTranslucentPriorityModifierComponentState& InOther) const
	{
		return PrimitiveComponentWeak.IsValid() && PrimitiveComponentWeak == InOther.PrimitiveComponentWeak;
	}

	/** Modifier managing this component state */
	UPROPERTY()
	TWeakObjectPtr<UAvaTranslucentPriorityModifier> ModifierWeak;

	/** Primitive component that this state is describing */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> PrimitiveComponentWeak;

	/** Previous sort priority to restore */
	UPROPERTY()
	int32 SortPriority = 0;
};

/**
 * Singleton class for translucent priority modifiers to share data about component state
 */
UCLASS()
class UAvaTranslucentPriorityModifierShared : public UActorModifierCoreSharedObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnLevelGlobalsChanged)
	FOnLevelGlobalsChanged OnLevelGlobalsChangedDelegate;

	/** Replaces the component linked to this context by new components set */
	void SetComponentsState(UAvaTranslucentPriorityModifier* InModifierContext, const TSet<TWeakObjectPtr<UPrimitiveComponent>>& InComponents);

	/** Save component state, adds it if it is not tracked, optionally override the context */
	void SaveComponentState(UAvaTranslucentPriorityModifier* InModifierContext, UPrimitiveComponent* InComponent, bool bInOverrideContext);

	/** Restore component state, removes it if no other modifier track that state */
	void RestoreComponentState(const UAvaTranslucentPriorityModifier* InModifierContext, UPrimitiveComponent* InComponent, bool bInClearState);

	/** Restore components state linked to this modifier */
	void RestoreComponentsState(const UAvaTranslucentPriorityModifier* InModifierContext, bool bInClearState);

	/** Restore components state linked to this modifier */
	void RestoreComponentsState(const UAvaTranslucentPriorityModifier* InModifierContext, const TSet<TWeakObjectPtr<UPrimitiveComponent>>& InComponents, bool bInClearState);

	/** Find the modifier linked to a component */
	UAvaTranslucentPriorityModifier* FindModifierContext(UPrimitiveComponent* InComponent) const;

	/** Get sorted components state based on modifier context */
	TArray<const FAvaTranslucentPriorityModifierComponentState*> GetSortedComponentStates(UAvaTranslucentPriorityModifier* InModifierContext) const;

	void SetSortPriorityOffset(int32 InOffset);

	int32 GetSortPriorityOffset() const
	{
		return SortPriorityOffset;
	}

	void SetSortPriorityStep(int32 InStep);

	int32 GetSortPriorityStep() const
	{
		return SortPriorityStep;
	}

private:
	//~ Begin UObject
	virtual void PostLoad() override;
	//~ End UObject

	UPROPERTY()
	TSet<FAvaTranslucentPriorityModifierComponentState> ComponentStates;

	/** Offset for the whole level */
	UPROPERTY()
	int32 SortPriorityOffset = 0;

	/** Incremental step for the whole level */
	UPROPERTY()
	int32 SortPriorityStep = 1;
};
