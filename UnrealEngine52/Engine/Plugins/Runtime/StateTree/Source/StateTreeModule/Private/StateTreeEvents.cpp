// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEvents.h"
#include "StateTreeTypes.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeEvents)

void FStateTreeEventQueue::SendEvent(const UObject* Owner, const FGameplayTag& Tag, const FConstStructView Payload, const FName Origin)
{
	if (Events.Num() >= MaxActiveEvents)
	{
		UE_VLOG_UELOG(Owner, LogStateTree, Error, TEXT("%s: Too many events send on '%s'. Dropping event %s"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner), *Tag.ToString());
		return;
	}

	Events.Emplace(Tag, Payload, Origin);
}
