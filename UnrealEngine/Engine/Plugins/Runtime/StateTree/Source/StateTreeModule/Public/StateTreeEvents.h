// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "StructView.h"
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

	explicit FStateTreeEvent(const FGameplayTag InTag)
		: Tag(InTag)
	{
	}
	
	explicit FStateTreeEvent(const FGameplayTag InTag, const FConstStructView InPayload, const FName InOrigin)
		: Tag(InTag)
		, Payload(InPayload)
		, Origin(InOrigin)
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

/**
 * Event queue buffering all the events to be processed by a State Tree.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeEventQueue
{
	GENERATED_BODY()

	/** Maximum number of events that can be buffered. */
	static constexpr int32 MaxActiveEvents = 64;

	/** @return const view to all the events in the buffer. */
	TConstArrayView<FStateTreeEvent> GetEvents() const
	{
		return Events;
	}

	/** Resets the events in the event queue */
	void Reset()
	{
		Events.Reset();
	}

	/**
	 * Buffers and event to be sent to the State Tree.
	 * @param Owner Optional pointer to an owner UObject that is used for logging errors.
	 * @param Tag tag identifying the event.
	 * @param Payload Optional reference to the payload struct.
	 * @param Origin Optional name identifying the origin of the event.
	 */
	void SendEvent(const UObject* Owner, const FGameplayTag& Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

protected:
	// Used by FStateTreeExecutionState to implement deprecated functionality.
	TArray<FStateTreeEvent>& GetEventsArray() { return Events; };

	UPROPERTY()
	TArray<FStateTreeEvent> Events;

	friend struct FStateTreeInstanceData;
};
