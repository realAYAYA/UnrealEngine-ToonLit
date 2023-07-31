// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "StateTreeEvents.generated.h"

/** Enum used for flow control during event iteration. */
UENUM()
enum class EStateTreeLoopEvents : uint8
{
	/** Continues to next event. */
	Next,
	/** Stops the event handling loop. */
	Break,
};

/**
 * StateTree event with payload.
 */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeEvent
{
	GENERATED_BODY()

	FStateTreeEvent() = default;
	FStateTreeEvent(const FGameplayTag& InTag)
		: Tag(InTag)
	{
	}
	
	/** Tag describing the event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta=(Categories="StateTreeEvent"))
	FGameplayTag Tag;

	/** Optional payload for the event. */ 
	UPROPERTY(EditAnywhere, Category = "Default")
	FInstancedStruct Payload;

	/** Optional info to describe who sent the event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	FName Origin;
};
