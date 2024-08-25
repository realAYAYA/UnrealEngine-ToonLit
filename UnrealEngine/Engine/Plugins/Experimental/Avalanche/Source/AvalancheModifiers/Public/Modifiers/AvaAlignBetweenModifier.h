// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaBaseModifier.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "AvaAlignBetweenModifier.generated.h"

class AActor;

/** Represents an actor with a weight and an enabled state. */
USTRUCT(BlueprintType)
struct FAvaAlignBetweenWeightedActor
{
	GENERATED_BODY()

	FAvaAlignBetweenWeightedActor()
	{}

	explicit FAvaAlignBetweenWeightedActor(AActor* InActor)
		: ActorWeak(InActor)
	{}

	explicit FAvaAlignBetweenWeightedActor(AActor* InActor, float InWeight, bool bInEnabled)
		: ActorWeak(InActor)
		, Weight(InWeight)
		, bEnabled(bInEnabled)
	{}

	/** Returns true if the actor is valid and the state is enabled. */
	bool IsValid() const
	{
		return ActorWeak.IsValid() && bEnabled;
	}

	friend uint32 GetTypeHash(const FAvaAlignBetweenWeightedActor& InItem)
	{
		return GetTypeHash(InItem.ActorWeak);
	}

	bool operator==(const FAvaAlignBetweenWeightedActor& Other) const
	{
		return ActorWeak == Other.ActorWeak;
	}

	/** An actor that will effect the placement location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	TWeakObjectPtr<AActor> ActorWeak;

	/** How much effect this actor has on the placement location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design", Meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Weight = 0.0f;

	/** If true, will consider this weighted actor when calculating the placement location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	bool bEnabled = false;
};

/**
 * Moves the modifying actor to the averaged location between an array of specified actors.
 */
UCLASS(MinimalAPI, BlueprintType)
class UAvaAlignBetweenModifier : public UAvaBaseModifier
	, public IAvaTransformUpdateHandler
{
	GENERATED_BODY()

public:
	/** Gets all actors from their reference actor structs. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AlignBetween")
	TSet<AActor*> GetActors(const bool bEnabledOnly = false) const;

	/** Returns all valid reference actors that enabled and have a weight greater than 0. */
	TSet<FAvaAlignBetweenWeightedActor> GetEnabledReferenceActors() const;

	/** Gets all reference actors and their weights. */
	UFUNCTION(BlueprintPure, Category = "Motion Design|Modifiers|AlignBetween")
	TSet<FAvaAlignBetweenWeightedActor> GetReferenceActors() const
	{
		return ReferenceActors;
	}

	/** Sets all reference actors and their weights. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AlignBetween")
	AVALANCHEMODIFIERS_API void SetReferenceActors(const TSet<FAvaAlignBetweenWeightedActor>& NewReferenceActors);

	/** Adds an actor to the reference list. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AlignBetween")
	void AddReferenceActor(const FAvaAlignBetweenWeightedActor& ReferenceActor);

	/** Removes an actor from the reference list. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AlignBetween")
	bool RemoveReferenceActor(AActor* const Actor);

	/** Finds an actor in the reference list. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design|Modifiers|AlignBetween")
	bool FindReferenceActor(AActor* InActor, FAvaAlignBetweenWeightedActor& OutReferenceActor) const;

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void Apply() override;
	virtual void OnModifiedActorTransformed() override;
	//~ End UActorModifierCoreBase


	//~ Begin IAvaTransformUpdatedExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override;
	//~ End IAvaTransformUpdatedExtension

	void OnReferenceActorsChanged();
	void SetTransformExtensionReferenceActors();

	/** Editable set of reference actors and weights used to calculate the average location for this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetReferenceActors", Getter="GetReferenceActors", Category="AlignBetween", meta=(AllowPrivateAccess="true"))
	TSet<FAvaAlignBetweenWeightedActor> ReferenceActors;
};
