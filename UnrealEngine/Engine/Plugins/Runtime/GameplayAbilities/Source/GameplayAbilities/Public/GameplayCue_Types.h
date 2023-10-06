// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "UObject/ObjectKey.h"
#include "GameplayEffect.h"
#include "GameplayCue_Types.generated.h"

class AGameplayCueNotify_Actor;
class UAbilitySystemComponent;
class UGameplayCueSet;


/** Describes what type of payload is attached to a cue execution, we only replicate what is needed */
UENUM()
enum class EGameplayCuePayloadType : uint8
{
	/** Uses FGameplayCueParameters */
	CueParameters,
	/** Uses FGameplayEffectSpecForRPC */
	FromSpec,
};


/** Structure to keep track of pending gameplay cues that haven't been applied yet. */
USTRUCT()
struct FGameplayCuePendingExecute
{
	GENERATED_USTRUCT_BODY()

	FGameplayCuePendingExecute() {}

	/** List of tags, we allocate one as there is almost always exactly one tag */
	TArray<FGameplayTag, TInlineAllocator<1> > GameplayCueTags;
	
	/** Prediction key that spawned this cue */
	UPROPERTY()
	FPredictionKey PredictionKey;

	/** What type of payload is attached to this cue */
	UPROPERTY()
	EGameplayCuePayloadType PayloadType = EGameplayCuePayloadType::CueParameters;

	/** What component to send the cue on */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwningComponent = nullptr;

	/** If this cue is from a spec, here's the copy of that spec */
	UPROPERTY()
	FGameplayEffectSpecForRPC FromSpec;

	/** Store the full cue parameters or just the effect context depending on type */
	UPROPERTY()
	FGameplayCueParameters CueParameters;
};

USTRUCT()
struct FGameplayCueNotifyActorArray
{
	GENERATED_BODY()

	UPROPERTY(transient)
	TArray<TObjectPtr<AGameplayCueNotify_Actor>> Actors;
};

/** Struct for pooling and preallocating gameplaycuenotify_actor classes. This data is per world and used to track what actors are available to recycle and which classes need to preallocate instances of those actors */
USTRUCT()
struct FPreallocationInfo
{
	GENERATED_USTRUCT_BODY()

	/** Raw list of pooled instances. This relies on NotifyGameplayCueActorEndPlay always being called when actor is destroyed */
	UPROPERTY(transient)
	TMap<TObjectPtr<UClass>, FGameplayCueNotifyActorArray> PreallocatedInstances;

	/** List of classes that will be pooled */
	UPROPERTY(transient)
	TArray<TSubclassOf<AGameplayCueNotify_Actor>> ClassesNeedingPreallocation;

	/** World that owns this list */
	FObjectKey OwningWorldKey;
};


/** Struct that is used by the gameplaycue manager to tie an instanced gameplaycue to the calling gamecode. Usually this is just the target actor, but can also be unique per instigator/sourceobject */
struct UE_DEPRECATED(5.3, "Look at FGCNotifyActorKey constructor notes for upgrade steps.") FGCNotifyActorKey
{
	FGCNotifyActorKey()
	{

	}

	// This class is deprecated.  It was previously used to index into a map to find an instance of AGameplayCueNotify_Actor which was in use.
	// Instead, you can find the AGameplayCueNotify_Actor this way:
	//	1. InTargetActor will be its parent.  So you search InTargetActor->Children for instances of AGameplayCueNotify_Actor.
	//	2. InCueClass can be inferred from AGameplayCueNotify_Actor->GetClass().
	//	3. AGameplayCueNotify_Actor has two new fields: CueInstigator (corresponds to InInstigatorActor) and CueSourceObject; (corresponds to InSourceObj).
	// Refer to UGameplayCueManager::GetInstancedCueActor.
	FGCNotifyActorKey(AActor* InTargetActor, UClass* InCueClass, AActor* InInstigatorActor=nullptr, const UObject* InSourceObj=nullptr)
	{
		TargetActor = FObjectKey(InTargetActor);
		OptionalInstigatorActor = FObjectKey(InInstigatorActor);
		OptionalSourceObject = FObjectKey(InSourceObj);
		CueClass = FObjectKey(InCueClass);
	}

	

	FObjectKey	TargetActor;
	FObjectKey	OptionalInstigatorActor;
	FObjectKey	OptionalSourceObject;
	FObjectKey	CueClass;

	FORCEINLINE bool operator==(const FGCNotifyActorKey& Other) const
	{
		return TargetActor == Other.TargetActor && CueClass == Other.CueClass &&
				OptionalInstigatorActor == Other.OptionalInstigatorActor && OptionalSourceObject == Other.OptionalSourceObject;
	}
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FORCEINLINE uint32 GetTypeHash(const FGCNotifyActorKey& Key)
{
	return GetTypeHash(Key.TargetActor)	^
			GetTypeHash(Key.OptionalInstigatorActor) ^
			GetTypeHash(Key.OptionalSourceObject) ^
			GetTypeHash(Key.CueClass);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/**
 *	FScopedGameplayCueSendContext
 *	Add this around code that sends multiple gameplay cues to allow grouping them into a smalkler number of cues for more efficient networking
 */
struct GAMEPLAYABILITIES_API FScopedGameplayCueSendContext
{
	FScopedGameplayCueSendContext();
	~FScopedGameplayCueSendContext();
};

/** Delegate for when GC notifies are added or removed from manager */
DECLARE_MULTICAST_DELEGATE(FOnGameplayCueNotifyChange);