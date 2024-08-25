// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifiers/ActorModifierCoreSharedObject.h"
#include "Modifiers/AvaBaseModifier.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AvaVisibilityModifierShared.generated.h"

class AActor;
class UAvaBaseModifier;

UENUM(meta=(Bitflags))
enum class EAvaVisibilityActor : uint8
{
	None          = 0,
	Game          = 1 << 0,
	Editor        = 1 << 1,
	GameAndEditor = Game | Editor
};
ENUM_CLASS_FLAGS(EAvaVisibilityActor);

USTRUCT()
struct FAvaVisibilitySharedModifierState
{
	GENERATED_BODY()

	FAvaVisibilitySharedModifierState() {}
	FAvaVisibilitySharedModifierState(UAvaBaseModifier* InModifier)
		: ModifierWeak(InModifier)
	{}

	/** Save this modifier state if valid */
	void Save(const AActor* InActor);

	/** Restore this modifier state if valid */
	void Restore(AActor* InActor) const;

	friend uint32 GetTypeHash(const FAvaVisibilitySharedModifierState& InItem)
	{
		return GetTypeHash(InItem.ModifierWeak);
	}

	bool operator==(const FAvaVisibilitySharedModifierState& Other) const
	{
		return ModifierWeak == Other.ModifierWeak;
	}

	UPROPERTY()
	TWeakObjectPtr<UAvaBaseModifier> ModifierWeak;

#if WITH_EDITORONLY_DATA
	/** Pre state editor visibility saved */
	UPROPERTY()
	bool bActorHiddenInEditor = false;
#endif

	/** Pre state game visibility saved */
	UPROPERTY()
	bool bActorHiddenInGame = false;
};

USTRUCT()
struct FAvaVisibilitySharedActorState
{
	GENERATED_BODY()

	FAvaVisibilitySharedActorState() {}
	FAvaVisibilitySharedActorState(AActor* InActor)
		: ActorWeak(InActor)
	{}

	/** Save this actor state if valid */
	void Save();

	/** Restore this actor state if valid */
	void Restore() const;

	friend uint32 GetTypeHash(const FAvaVisibilitySharedActorState& InItem)
	{
		return GetTypeHash(InItem.ActorWeak);
	}

	bool operator==(const FAvaVisibilitySharedActorState& Other) const
	{
		return ActorWeak == Other.ActorWeak;
	}

	/** Modifiers that are currently watching this state and locking it */
	UPROPERTY()
	TSet<FAvaVisibilitySharedModifierState> ModifierStates;

	/** Actor that this state is describing */
	UPROPERTY()
	TWeakObjectPtr<AActor> ActorWeak;

#if WITH_EDITORONLY_DATA
	/** Pre state editor visibility saved */
	UPROPERTY()
	bool bActorHiddenInEditor = false;
#endif

	/** Pre state game visibility saved */
	UPROPERTY()
	bool bActorHiddenInGame = false;
};

/**
 * Singleton class for visibility modifiers to share data between each other
 * Used because multiple modifier could be watching/updating an actor
 * We want to save the state of that actor once before any modifier changes it
 * and restore it when no other modifier is watching it
 */
UCLASS()
class UAvaVisibilityModifierShared : public UActorModifierCoreSharedObject
{
	GENERATED_BODY()

public:
	/** Watch actor state, adds it if it is not tracked */
	void SaveActorState(UAvaBaseModifier* InModifierContext, AActor* InActor);

	/** Unwatch actor state, removes it if no other modifier track that actor state */
	void RestoreActorState(UAvaBaseModifier* InModifierContext, AActor* InActor);

	/** Gather original state before any modifier is applied if there is one */
	FAvaVisibilitySharedActorState* FindActorState(AActor* InActor);

	/** Set actor visibility in game or editor and recurse, tracks original state if not tracked */
	void SetActorVisibility(UAvaBaseModifier* InModifierContext, AActor* InActor, bool bInHidden, bool bInRecurse = false, EAvaVisibilityActor InActorVisibility = EAvaVisibilityActor::GameAndEditor);

	/** Set actors visibility in game or editor, tracks original state if not tracked */
	void SetActorsVisibility(UAvaBaseModifier* InModifierContext, TArray<AActor*> InActors, bool bInHidden, EAvaVisibilityActor InActorVisibility = EAvaVisibilityActor::GameAndEditor);

	/** Unwatch all actors states linked to this modifier */
	void RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<AActor*>* InActors = nullptr);

	void RestoreActorsState(UAvaBaseModifier* InModifierContext, const TSet<TWeakObjectPtr<AActor>>& InActors);

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
	TSet<FAvaVisibilitySharedActorState> ActorStates;
};
