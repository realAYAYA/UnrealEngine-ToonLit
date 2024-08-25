// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleContext.h"
#include "AnimNextSchedulerEntry.h"

namespace UE::AnimNext
{

UObject* FScheduleContext::GetContextObject() const
{
	if(Entry)
	{
		return Entry->ResolvedObject;
	}
	else
	{
		return ContextObject;
	}
}

float FScheduleContext::GetDeltaTime() const 
{ 
	check(Entry != nullptr);
	return Entry->DeltaTime; 
}

}
