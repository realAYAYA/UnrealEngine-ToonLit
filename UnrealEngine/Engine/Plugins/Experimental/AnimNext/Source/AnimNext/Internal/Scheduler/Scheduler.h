// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Scheduler/ScheduleHandle.h"

enum class EAnimNextScheduleInitMethod : uint8;
class UAnimNextGraph;
class UAnimNextParameterBlock;
class UAnimNextSchedule;

namespace UE::AnimNext
{
	struct FScheduler;
	struct FSchedulerImpl;
	struct FSchedulePortDefinition;
	struct FParamStackLayerHandle;
	struct FScheduleContext;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext
{

// Main interface into the AnimNext scheduling system
struct FScheduler
{
	// Start up the scheduler system
	static void Init();

	// Shut down the scheduler system
	static void Destroy();

	// Acquire a handle that binds a schedule
	// Initial parameter binding can be achieved via InitializeCallback, which is called once the schedule's data
	// structures have been set up, but before it is first run.
	static FScheduleHandle AcquireHandle(UObject* InObject, UAnimNextSchedule* InSchedule, EAnimNextScheduleInitMethod InInitMethod, TUniqueFunction<void(const FScheduleContext&)>&& InitializeCallback = nullptr);

	// Release an already acquired handle
	// The full release of the binding referenced by the handle map be deferred after this call is made
	static void ReleaseHandle(UObject* InObject, FScheduleHandle& InHandle);

	// Enables or disables the schedule parameterization represented by the supplied handle
	// This operation is deferred until the next time the schedule ticks
	static void EnableHandle(UObject* InObject, FScheduleHandle InHandle, bool bInEnabled);

	enum class ETaskRunLocation : int32
	{
		// Run the task before the specified task
		Before,

		// Run the task after the specified task
		After,
	};

	// Queue a task to run at a particular point in a schedule
	// @param	InHandle		The handle to queue the task to
	// @param	InTaskName		The name of the task in the schedule to run the supplied task relative to
	// @param	InTaskFunction	The function to run
	// @param	InLocation		Where to run the task, before or after
	static void QueueTask(UObject* InObject, FScheduleHandle InHandle, FName InScheduleTaskName, TUniqueFunction<void(const FScheduleContext&)>&& InTaskFunction, ETaskRunLocation InLocation = ETaskRunLocation::Before);

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

#if WITH_EDITOR
	// Refresh any entries that use the provided schedule as it has been recompiled.
	static ANIMNEXT_API void OnScheduleCompiled(UAnimNextSchedule* InSchedule);
#endif
};

}