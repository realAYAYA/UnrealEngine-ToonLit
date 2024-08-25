// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamStack.h"
#include "Scheduler/ScheduleHandle.h"
#include "Scheduler/ScheduleInstanceData.h"

class UAnimNextSchedule;

namespace UE::AnimNext
{

struct FScheduleContext
{
public:
	FScheduleContext() = default;

	FScheduleContext(FScheduleContext&& InOther) = default;
	FScheduleContext& operator=(FScheduleContext&& InOther) = default;
	FScheduleContext(const FScheduleContext& InOther) = delete;
	FScheduleContext& operator=(const FScheduleContext& InOther) = delete;

	FScheduleContext(const UAnimNextSchedule* InSchedule, FAnimNextSchedulerEntry* InEntry)
		: Schedule(InSchedule)
		, Entry(InEntry)
	{}

	explicit FScheduleContext(UObject* InContextObject)
		: ContextObject(InContextObject)
	{}
	
	// Get a string that can identify this schedule for debugging
	FString GetDebugString() const 
	{ 
		if(ensure(InstanceData))
		{
			return InstanceData->Handle.ToString(); 
		}
		return TEXT("Invalid schedule instance data");
	}

	// Get the instance data for this running schedule
	FScheduleInstanceData& GetInstanceData() const 
	{ 
		check(InstanceData.IsValid());
		return *InstanceData.Get(); 
	}

	// Get the current delta time - only valid if we are within a running schedule
	float GetDeltaTime() const;

	// Get the current object context that the schedule is running in 
	UObject* GetContextObject() const;

	// Current schedule
	const UAnimNextSchedule* Schedule = nullptr;

	// Current entry for this schedule
	FAnimNextSchedulerEntry* Entry = nullptr;

	// Instance data for the schedule
	TUniquePtr<FScheduleInstanceData> InstanceData;

	// Context object - only valid when we dont have a running schedule
	UObject* ContextObject = nullptr;
};

}
