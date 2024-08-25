// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "UObject/ScriptInterface.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameplayCueInterface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "AbilitySystemBlueprintLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UGameplayEffectUIData;

/** Blueprint library for ability system. Many of these functions are useful to call from native as well */
UCLASS(meta=(ScriptName="AbilitySystemLibrary"))
class GAMEPLAYABILITIES_API UAbilitySystemBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Tries to find an ability system component on the actor, will use AbilitySystemInterface or fall back to a component search */
	UFUNCTION(BlueprintPure, Category = Ability, Meta=(DefaultToSelf = "Actor"))
	static UAbilitySystemComponent* GetAbilitySystemComponent(AActor* Actor);

	/**
	 * This function can be used to trigger an ability on the actor in question with useful payload data.
	 * NOTE: GetAbilitySystemComponent is called on the actor to find a good component, and if the component isn't
	 * found, the event will not be sent.
	 */
	UFUNCTION(BlueprintCallable, Category = Ability, Meta = (Tooltip = "This function can be used to trigger an ability on the actor in question with useful payload data."))
	static void SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload);
	
	// -------------------------------------------------------------------------------
	//		Attribute
	// -------------------------------------------------------------------------------

	/** Returns true if the attribute actually exists */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static bool IsValid(FGameplayAttribute Attribute);

	/** Returns the value of Attribute from the ability system component belonging to Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttribute(const AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the value of Attribute from the ability system component AbilitySystem. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeFromAbilitySystemComponent(const UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the base value of Attribute from the ability system component belonging to Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeBase(const AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the base value of Attribute from the ability system component AbilitySystemComponent. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeBaseFromAbilitySystemComponent(const UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the value of Attribute from the ability system component AbilitySystem after evaluating it with source and target tags. bSuccess indicates the success or failure of this operation. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float EvaluateAttributeValueWithTags(UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, bool& bSuccess);

	/** Returns the value of Attribute from the ability system component AbilitySystem after evaluating it with source and target tags using the passed in base value instead of the real base value. bSuccess indicates the success or failure of this operation. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float EvaluateAttributeValueWithTagsAndBase(UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, float BaseValue, bool& bSuccess);

	/** Simple equality operator for gameplay attributes */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Gameplay Attribute)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Ability|Attribute")
	static bool EqualEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute AttributeA, FGameplayAttribute AttributeB);

	/** Simple inequality operator for gameplay attributes */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (Gameplay Attribute)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Ability|Attribute")
	static bool NotEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute AttributeA, FGameplayAttribute AttributeB);

	/** Returns FString representation of a gameplay attribute's set class and name, in the form of AttrSetName.AttrName (or just AttrName if not part of a set).  */
	UFUNCTION(BlueprintPure, Category="Ability|Attribute")
	static FString GetDebugStringFromGameplayAttribute(const FGameplayAttribute& Attribute);

	// -------------------------------------------------------------------------------
	//		TargetData
	// -------------------------------------------------------------------------------

	/** Copies targets from HandleToAdd to TargetHandle */
	UFUNCTION(BlueprintCallable, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle AppendTargetDataHandle(FGameplayAbilityTargetDataHandle TargetHandle, const FGameplayAbilityTargetDataHandle& HandleToAdd);

	/** Creates a target data with a source and destination location */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromLocations(const FGameplayAbilityTargetingLocationInfo& SourceLocation, const FGameplayAbilityTargetingLocationInfo& TargetLocation);

	/** Creates a target data with a single hit result */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromHitResult(const FHitResult& HitResult);

	/** Returns number of target data objects, not necessarily number of distinct targets */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static int32 GetDataCountFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Creates single actor target data */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromActor(AActor* Actor);

	/** Creates actor array target data */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromActorArray(const TArray<AActor*>& ActorArray, bool OneTargetPerHandle);

	/** Create a new target data handle with filtration performed on the data */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	FilterTargetData(const FGameplayAbilityTargetDataHandle& TargetDataHandle, FGameplayTargetDataFilterHandle ActorFilterClass);

	/** Create a handle for filtering target data, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Filter")
	static FGameplayTargetDataFilterHandle MakeFilterHandle(FGameplayTargetDataFilter Filter, AActor* FilterActor);

	/** Create a spec handle, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Spec")
	static FGameplayEffectSpecHandle MakeSpecHandle(UGameplayEffect* InGameplayEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel = 1.0f);

	/** Create a spec handle, cloning another */
	UFUNCTION(BlueprintPure, Category = "Spec")
	static FGameplayEffectSpecHandle CloneSpecHandle(AActor* InNewInstigator, AActor* InEffectCauser, FGameplayEffectSpecHandle GameplayEffectSpecHandle_Clone);

	/** Returns all actors targeted, for a given index */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static TArray<AActor*> GetActorsFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns all actors targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static TArray<AActor*> GetAllActorsFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Returns true if the given TargetData has the actor passed in targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool DoesTargetDataContainActor(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index, AActor* Actor);

	/** Returns true if the given TargetData has at least 1 actor targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasActor(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns true if the target data has a hit result */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasHitResult(const FGameplayAbilityTargetDataHandle& HitResult, int32 Index);

	/** Returns the hit result for a given index if it exists */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FHitResult GetHitResultFromTargetData(const FGameplayAbilityTargetDataHandle& HitResult, int32 Index);

	/** Returns true if the target data has an origin */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasOrigin(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns the origin for a given index if it exists */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataOrigin(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns true if the target data has an end point */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasEndPoint(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns the end point for a given index if it exists */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FVector GetTargetDataEndPoint(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns the end point transform for a given index if it exists */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataEndPointTransform(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	// -------------------------------------------------------------------------------
	//		GameplayEffectContext
	// -------------------------------------------------------------------------------

	/** Returns true if this context has ever been initialized */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsValid"))
	static bool EffectContextIsValid(FGameplayEffectContextHandle EffectContext);

	/** Returns true if the ability system component that instigated this is locally controlled */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsInstigatorLocallyControlled"))
	static bool EffectContextIsInstigatorLocallyControlled(FGameplayEffectContextHandle EffectContext);

	/** Extracts a hit result from the effect context if it is set */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetHitResult"))
	static FHitResult EffectContextGetHitResult(FGameplayEffectContextHandle EffectContext);

	/** Returns true if there is a valid hit result inside the effect context */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "HasHitResult"))
	static bool EffectContextHasHitResult(FGameplayEffectContextHandle EffectContext);

	/** Adds a hit result to the effect context */
	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "AddHitResult"))
	static void EffectContextAddHitResult(FGameplayEffectContextHandle EffectContext, FHitResult HitResult, bool bReset);

	/** Gets the location the effect originated from */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOrigin"))
	static FVector EffectContextGetOrigin(FGameplayEffectContextHandle EffectContext);

	/** Sets the location the effect originated from */
	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "SetOrigin"))
	static void EffectContextSetOrigin(FGameplayEffectContextHandle EffectContext, FVector Origin);

	/** Gets the instigating actor (that holds the ability system component) of the EffectContext */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetInstigatorActor"))
	static AActor* EffectContextGetInstigatorActor(FGameplayEffectContextHandle EffectContext);

	/** Gets the original instigator actor that started the chain of events to cause this effect */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOriginalInstigatorActor"))
	static AActor* EffectContextGetOriginalInstigatorActor(FGameplayEffectContextHandle EffectContext);

	/** Gets the physical actor that caused the effect, possibly a projectile or weapon */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetEffectCauser"))
	static AActor* EffectContextGetEffectCauser(FGameplayEffectContextHandle EffectContext);

	/** Gets the source object of the effect. */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetSourceObject"))
	static UObject* EffectContextGetSourceObject(FGameplayEffectContextHandle EffectContext);

	// -------------------------------------------------------------------------------
	//		GameplayCue
	// -------------------------------------------------------------------------------

	/** Returns true if the ability system component that spawned this cue is locally controlled */
	UFUNCTION(BlueprintPure, Category="Ability|GameplayCue")
	static bool IsInstigatorLocallyControlled(FGameplayCueParameters Parameters);

	/** Returns true if the ability system component that spawned this cue is locally controlled and a player */
	UFUNCTION(BlueprintPure, Category="Ability|GameplayCue")
	static bool IsInstigatorLocallyControlledPlayer(FGameplayCueParameters Parameters);

	/** Returns number of actors stored in the Effect Context used by this cue */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static int32 GetActorCount(FGameplayCueParameters Parameters);

	/** Returns actor stored in the Effect Context used by this cue */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static AActor* GetActorByIndex(FGameplayCueParameters Parameters, int32 Index);

	/** Returns a hit result stored in the effect context if valid */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FHitResult GetHitResult(FGameplayCueParameters Parameters);

	/** Checks if the effect context has a hit reslt stored inside */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool HasHitResult(FGameplayCueParameters Parameters);

	/** Forwards the gameplay cue to another gameplay cue interface object */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayCue")
	static void ForwardGameplayCueToTarget(TScriptInterface<IGameplayCueInterface> TargetCueInterface, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Gets the instigating actor (that holds the ability system component) of the GameplayCue */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static AActor* GetInstigatorActor(FGameplayCueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FTransform GetInstigatorTransform(FGameplayCueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FVector GetOrigin(FGameplayCueParameters Parameters);

	/** Gets the best end location and normal for this gameplay cue. If there is hit result data, it will return this. Otherwise it will return the target actor's location/rotation. If none of this is available, it will return false. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool GetGameplayCueEndLocationAndNormal(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Location, FVector& Normal);

	/** Gets the best normalized effect direction for this gameplay cue. This is useful for effects that require the direction of an enemy attack. Returns true if a valid direction could be calculated. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool GetGameplayCueDirection(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Direction);

	/** Returns true if the aggregated source and target tags from the effect spec meets the tag requirements */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool DoesGameplayCueMeetTagRequirements(FGameplayCueParameters Parameters, const FGameplayTagRequirements& SourceTagReqs, const FGameplayTagRequirements& TargetTagReqs);

	/** Native make, to avoid having to deal with quantized vector types */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue", meta = (NativeMakeFunc, AdvancedDisplay=5, Location="0,0,0", Normal= "0,0,0", GameplayEffectLevel = "1", AbilityLevel = "1"))
	static FGameplayCueParameters MakeGameplayCueParameters(float NormalizedMagnitude, float RawMagnitude, FGameplayEffectContextHandle EffectContext, FGameplayTag MatchedTagName, FGameplayTag OriginalTag, FGameplayTagContainer AggregatedSourceTags, FGameplayTagContainer AggregatedTargetTags, FVector Location, FVector Normal, AActor* Instigator, AActor* EffectCauser, UObject* SourceObject, UPhysicalMaterial* PhysicalMaterial, int32 GameplayEffectLevel, int32 AbilityLevel, USceneComponent* TargetAttachComponent, bool bReplicateLocationWhenUsingMinimalRepProxy);

	/** Native break, to avoid having to deal with quantized vector types */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue", meta = (NativeBreakFunc, AdvancedDisplay=6))
	static void BreakGameplayCueParameters(const struct FGameplayCueParameters& Parameters, float& NormalizedMagnitude, float& RawMagnitude, FGameplayEffectContextHandle& EffectContext, FGameplayTag& MatchedTagName, FGameplayTag& OriginalTag, FGameplayTagContainer& AggregatedSourceTags, FGameplayTagContainer& AggregatedTargetTags, FVector& Location, FVector& Normal, AActor*& Instigator, AActor*& EffectCauser, UObject*& SourceObject, UPhysicalMaterial*& PhysicalMaterial, int32& GameplayEffectLevel, int32& AbilityLevel, USceneComponent*& TargetAttachComponent, bool& bReplicateLocationWhenUsingMinimalRepProxy);

	// -------------------------------------------------------------------------------
	//		GameplayEffectSpec
	// -------------------------------------------------------------------------------

	/** Sets a raw name Set By Caller magnitude value, the tag version should normally be used */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AssignSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FName DataName, float Magnitude);

	/** Sets a gameplay tag Set By Caller magnitude value */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect", meta = (GameplayTagFilter = "SetByCaller"))
	static FGameplayEffectSpecHandle AssignTagSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FGameplayTag DataTag, float Magnitude);

	/** Manually sets the duration on a specific effect */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetDuration(FGameplayEffectSpecHandle SpecHandle, float Duration);

	/** This instance of the effect will now grant NewGameplayTag to the object that this effect is applied to */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddGrantedTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag);

	/** This instance of the effect will now grant NewGameplayTags to the object that this effect is applied to */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddGrantedTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags);

	/** Adds NewGameplayTag to this instance of the effect */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddAssetTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag);

	/** Adds NewGameplayTags to this instance of the effect */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddAssetTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags);

	/** Adds LinkedGameplayEffectSpec to SpecHandles. LinkedGameplayEffectSpec will be applied when/if SpecHandle is applied successfully. LinkedGameplayEffectSpec will not be modified here. Returns the ORIGINAL SpecHandle (legacy decision) */
	UE_DEPRECATED(5.3, "Linked GameplayEffects aren't replicated.  The new UAdditionalGameplayEffectsComponent renders this functionality obsolete (and since it's configured in the Asset, it is properly synced).")
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect", meta = (DeprecatedFunction, DeprecatedMessage="Linked GameplayEffects aren't replicated.  Configure the GameplayEffect asset with a suitable GameplayEffectComponent."))
	static FGameplayEffectSpecHandle AddLinkedGameplayEffectSpec(FGameplayEffectSpecHandle SpecHandle, FGameplayEffectSpecHandle LinkedGameplayEffectSpec);

	/** Adds LinkedGameplayEffect to SpecHandles. LinkedGameplayEffectSpec will be applied when/if SpecHandle is applied successfully. This will initialize the LinkedGameplayEffect's Spec for you. Returns to NEW linked spec in case you want to add more to it. */
	UE_DEPRECATED(5.3, "Linked GameplayEffects aren't replicated.  The new UAdditionalGameplayEffectsComponent renders this functionality obsolete (and since it's configured in the Asset, it is properly synced).")
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect", meta = (DeprecatedFunction, DeprecatedMessage = "Linked GameplayEffects aren't replicated.  Configure the GameplayEffect asset with a suitable GameplayEffectComponent."))
	static FGameplayEffectSpecHandle AddLinkedGameplayEffect(FGameplayEffectSpecHandle SpecHandle, TSubclassOf<UGameplayEffect> LinkedGameplayEffect);

	/** Sets the GameplayEffectSpec's StackCount to the specified amount (prior to applying) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetStackCount(FGameplayEffectSpecHandle SpecHandle, int32 StackCount);

	/** Sets the GameplayEffectSpec's StackCount to the max stack count defined in the GameplayEffect definition */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetStackCountToMax(FGameplayEffectSpecHandle SpecHandle);

	/** Gets the GameplayEffectSpec's effect context handle */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectContextHandle GetEffectContext(FGameplayEffectSpecHandle SpecHandle);

	/** Returns handles for all Linked GE Specs that SpecHandle may apply. Useful if you want to append additional information to them. */
	UE_DEPRECATED(5.3, "Linked GameplayEffects aren't replicated.  The new UAdditionalGameplayEffectsComponent renders this functionality obsolete (and since it's configured in the Asset, it is properly synced).")
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DeprecatedFunction, DeprecatedMessage = "Linked GameplayEffects aren't replicated.  Configure the GameplayEffect asset with a suitable GameplayEffectComponent."))
	static TArray<FGameplayEffectSpecHandle> GetAllLinkedGameplayEffectSpecHandles(FGameplayEffectSpecHandle SpecHandle);

	/** Manually adds a set of tags to a given actor, and optionally replicates them. */
	UFUNCTION(BlueprintCallable, Category="Ability|GameplayEffect")
	static bool AddLooseGameplayTags(AActor* Actor, const FGameplayTagContainer& GameplayTags, bool bShouldReplicate=false);

	/** Manually removes a set of tags from a given actor, with optional replication. */
	UFUNCTION(BlueprintCallable, Category="Ability|GameplayEffect")
	static bool RemoveLooseGameplayTags(AActor* Actor, const FGameplayTagContainer& GameplayTags, bool bShouldReplicate=false);

	// -------------------------------------------------------------------------------
	//		GameplayEffectSpec
	// -------------------------------------------------------------------------------

	/** Gets the magnitude of change for an attribute on an APPLIED GameplayEffectSpec. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetModifiedAttributeMagnitude(FGameplayEffectSpecHandle SpecHandle, FGameplayAttribute Attribute);

	/** Helper function that may be useful to call from native as well */
	static float GetModifiedAttributeMagnitude(const FGameplayEffectSpec& SpecHandle, FGameplayAttribute Attribute);

	// -------------------------------------------------------------------------------
	//		FActiveGameplayEffectHandle
	// -------------------------------------------------------------------------------
	
	/** Returns current stack count of an active Gameplay Effect. Will return 0 if the GameplayEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static int32 GetActiveGameplayEffectStackCount(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns stack limit count of an active Gameplay Effect. Will return 0 if the GameplayEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static int32 GetActiveGameplayEffectStackLimitCount(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the start time (time which the GE was added) for a given GameplayEffect */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetActiveGameplayEffectStartTime(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the expected end time (when we think the GE will expire) for a given GameplayEffect (note someone could remove or change it before that happens!) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetActiveGameplayEffectExpectedEndTime(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the total duration for a given GameplayEffect */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetActiveGameplayEffectTotalDuration(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the total duration for a given GameplayEffect, basically ExpectedEndTime - Current Time */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect", meta = (WorldContext = "WorldContextObject"))
	static float GetActiveGameplayEffectRemainingDuration(UObject* WorldContextObject, FActiveGameplayEffectHandle ActiveHandle);

	/** Returns a debug string for display */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", Meta = (DisplayName = "Get Active GameplayEffect Debug String "))
	static FString GetActiveGameplayEffectDebugString(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the UI data for a gameplay effect class (if any) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect", Meta = (DisplayName = "Get GameplayEffect UI Data", DeterminesOutputType="DataType"))
	static const UGameplayEffectUIData* GetGameplayEffectUIData(TSubclassOf<UGameplayEffect> EffectClass, TSubclassOf<UGameplayEffectUIData> DataType);

	/** Equality operator for two Active Gameplay Effect Handles */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DisplayName = "Equal (Active Gameplay Effect Handle)", CompactNodeTitle = "==", ScriptOperator = "=="))
	static bool EqualEqual_ActiveGameplayEffectHandle(const FActiveGameplayEffectHandle& A, const FActiveGameplayEffectHandle& B);

	/** Inequality operator for two Active Gameplay Effect Handles */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DisplayName = "Not Equal (Active Gameplay Effect Handle)", CompactNodeTitle = "!=", ScriptOperator = "!="))
	static bool NotEqual_ActiveGameplayEffectHandle(const FActiveGameplayEffectHandle& A, const FActiveGameplayEffectHandle& B);

	/**
	 * Returns the Gameplay Effect CDO from an active handle.
	 * This reference should be considered read only,
	 * but you can use it to read additional Gameplay Effect info, such as icon, description, etc. 
	 */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect")
	static const UGameplayEffect* GetGameplayEffectFromActiveEffectHandle(const FActiveGameplayEffectHandle& ActiveHandle);

	// -------------------------------------------------------------------------------
	//		Gameplay Effect
	// -------------------------------------------------------------------------------

	/** Returns all tags that the Gameplay Effect *has* (that denote the GE Asset itself) and *does not* grant to any Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DisplayName = "Get Asset Tags"))
	static const FGameplayTagContainer& GetGameplayEffectAssetTags(TSubclassOf<UGameplayEffect> EffectClass);

	/** Returns all tags that the Gameplay Effect grants to the target Actor */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DisplayName = "Get Granted Tags"))
	static const FGameplayTagContainer& GetGameplayEffectGrantedTags(TSubclassOf<UGameplayEffect> EffectClass);

	// -------------------------------------------------------------------------------
	//		GameplayAbility
	// -------------------------------------------------------------------------------

	/**
	 * Provides the Gameplay Ability object associated with an Ability Spec Handle
	 * This can be either an instanced ability, or in the case of shared abilities, the Class Default Object
	 * 
	 * @param AbilitySpec The Gameplay Ability Spec you want to get the object from
	 * @param bIsInstance Set to true if this is an instanced ability instead of a shared CDO
	 * 
	 * @return Pointer to the Gameplay Ability object
	 */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static const UGameplayAbility* GetGameplayAbilityFromSpecHandle(UAbilitySystemComponent* AbilitySystem, const FGameplayAbilitySpecHandle& AbilitySpecHandle, bool& bIsInstance);

	/** Equality operator for two Gameplay Ability Spec Handles */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DisplayName = "Equal (Gameplay Ability Spec Handle)", CompactNodeTitle = "==", ScriptOperator = "=="))
	static bool EqualEqual_GameplayAbilitySpecHandle(const FGameplayAbilitySpecHandle& A, const FGameplayAbilitySpecHandle& B);

	/** Inequality operator for two Gameplay Ability Spec Handles */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", meta = (DisplayName = "Not Equal (Gameplay Ability Spec Handle)", CompactNodeTitle = "!=", ScriptOperator = "!="))
	static bool NotEqual_GameplayAbilitySpecHandle(const FGameplayAbilitySpecHandle& A, const FGameplayAbilitySpecHandle& B);
};
