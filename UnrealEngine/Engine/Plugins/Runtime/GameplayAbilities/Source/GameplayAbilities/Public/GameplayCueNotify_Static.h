// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueNotify_Static.generated.h"

/**
 *	A non instantiated UObject that acts as a handler for a GameplayCue. These are useful for one-off "burst" effects.
 */

UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Replication))
class GAMEPLAYABILITIES_API UGameplayCueNotify_Static : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Does this GameplayCueNotify handle this type of GameplayCueEvent? */
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const;

	virtual void OnOwnerDestroyed();

	virtual void PostInitProperties() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** Generic Event Graph event that will get called for every event type */
	UFUNCTION(BlueprintImplementableEvent, Category = "GameplayCueNotify", DisplayName = "HandleGameplayCue", meta=(ScriptName = "HandleGameplayCue"))
	void K2_HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	/** Called when a GameplayCue is executed, this is used for instant effects or periodic ticks */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool OnExecute(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	/** Called when a GameplayCue with duration is first activated, this will only be called if the client witnessed the activation */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool OnActive(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	/** Called when a GameplayCue with duration is first seen as active, even if it wasn't actually just applied (Join in progress, etc) */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool WhileActive(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	/** Called when a GameplayCue with duration is removed */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool OnRemove(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	/** Tag this notify is activated by */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue, meta=(Categories="GameplayCue"))
	FGameplayTag	GameplayCueTag;

	/** Mirrors GameplayCueTag in order to be asset registry searchable */
	UPROPERTY(AssetRegistrySearchable)
	FName GameplayCueName;

	/** Does this Cue override other cues, or is it called in addition to them? E.g., If this is Damage.Physical.Slash, we wont call Damage.Physical afer we run this cue. */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	bool IsOverride;

private:
	virtual void DeriveGameplayCueTagFromAssetName();
};
