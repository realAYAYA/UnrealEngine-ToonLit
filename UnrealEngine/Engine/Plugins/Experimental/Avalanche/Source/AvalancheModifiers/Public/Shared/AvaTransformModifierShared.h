// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifiers/ActorModifierCoreSharedObject.h"
#include "Modifiers/AvaBaseModifier.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AvaTransformModifierShared.generated.h"

class AActor;
class UAvaBaseModifier;

UENUM()
enum class EAvaTransformSharedModifier : uint8
{
	None,
	Location = 1 << 0,
	Rotation = 1 << 1,
	Scale = 1 << 2,
	LocationRotation = Location | Rotation,
	LocationScale = Location | Scale,
	RotationScale = Rotation | Scale,
	All = Location | Rotation | Scale
};

USTRUCT()
struct FAvaTransformSharedModifierState
{
	GENERATED_BODY()

	FAvaTransformSharedModifierState() {}
	FAvaTransformSharedModifierState(UAvaBaseModifier* InModifier)
		: ModifierWeak(InModifier)
	{}

	/** Save this modifier state if valid */
	void Save(const AActor* InActor);

	/** Restore this modifier state if valid */
	void Restore(AActor* InActor, EAvaTransformSharedModifier InRestoreState) const;

	friend uint32 GetTypeHash(const FAvaTransformSharedModifierState& InItem)
	{
		return GetTypeHash(InItem.ModifierWeak);
	}

	bool operator==(const FAvaTransformSharedModifierState& Other) const
	{
		return ModifierWeak == Other.ModifierWeak;
	}

	UPROPERTY()
	TWeakObjectPtr<UAvaBaseModifier> ModifierWeak;

	UPROPERTY()
	FTransform ActorTransform;
};

USTRUCT()
struct FAvaTransformSharedActorState
{
	GENERATED_BODY()

	FAvaTransformSharedActorState() {}
	FAvaTransformSharedActorState(AActor* InActor)
		: ActorWeak(InActor)
	{}

	/** Save this actor state if valid */
	void Save();

	/** Restore this actor state if valid */
	void Restore(EAvaTransformSharedModifier InRestoreState) const;

	friend uint32 GetTypeHash(const FAvaTransformSharedActorState& InItem)
	{
		return GetTypeHash(InItem.ActorWeak);
	}

	bool operator==(const FAvaTransformSharedActorState& Other) const
	{
		return ActorWeak == Other.ActorWeak;
	}

	/** Modifiers that are currently watching this state and locking it */
	UPROPERTY()
	TSet<FAvaTransformSharedModifierState> ModifierStates;

	/** Actor that this state is describing */
	UPROPERTY()
	TWeakObjectPtr<AActor> ActorWeak;

	/** Pre state transform saved */
	UPROPERTY()
	FTransform ActorTransform;
};

/**
 * Singleton class for transform modifiers to share data between each other
 * Used because multiple modifier could be watching/updating an actor
 * We want to save the state of that actor once before any modifier changes it
 * and restore it when no other modifier is watching it
 */
UCLASS()
class UAvaTransformModifierShared : public UActorModifierCoreSharedObject
{
	GENERATED_BODY()

public:
	/** Save actor state, adds it if it is not tracked */
	void SaveActorState(UAvaBaseModifier* InModifierContext, AActor* InActor);

	/** Restore actor state, removes it if no other modifier track that actor state */
	void RestoreActorState(UAvaBaseModifier* InModifierContext, AActor* InActor, EAvaTransformSharedModifier InRestoreTransform = EAvaTransformSharedModifier::All);

	/** Get the actor state for a specific actor */
	FAvaTransformSharedActorState* FindActorState(AActor* InActor);

	/** Get all actor state related to a modifier */
	TSet<FAvaTransformSharedActorState*> FindActorsState(UAvaBaseModifier* InModifierContext);

	/** Restore all actors states linked to this modifier */
	void RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<AActor*>* InActors = nullptr, EAvaTransformSharedModifier InRestoreTransform = EAvaTransformSharedModifier::All);

	/** Restore all specified actors linked to this modifier */
	void RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<TWeakObjectPtr<AActor>>& InActors, EAvaTransformSharedModifier InRestoreTransform = EAvaTransformSharedModifier::All);

	/** Returns true, if this modifier is tracking this actor */
	bool IsActorStateSaved(UAvaBaseModifier* InModifierContext, AActor* InActor);

	/** Returns true, if this modifier is tracking any actor */
	bool IsActorsStateSaved(UAvaBaseModifier* InModifierContext);

protected:
	//~ Begin UObject
	virtual void PostLoad() override;
	//~ End UObject

	/** Actor state before any modifier applied to it */
	UPROPERTY()
	TSet<FAvaTransformSharedActorState> ActorStates;
};
