// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Scheduler/ScheduleHandle.h"
#include "ScheduleContext.h"
#include "Param/ParamStack.h"
#include "Param/ParamStackLayerHandle.h"
#include "Tasks/Task.h"
#include "Scheduler/ScheduleTickFunction.h"
#include "Scheduler/AnimNextSchedulerEntry.h"
#include "Scheduler/Scheduler.h"
#include "AnimNextSchedulerWorldSubsystem.generated.h"

class UAnimNextSchedule;
class UAnimNextParameterBlock;

namespace UE::AnimNext
{
	struct FScheduleHandle;
	struct FParamStackLayerHandle;
}

namespace UE::AnimNext
{

// A queued action to complete next frame
struct FSchedulePendingAction
{
	enum class EType : int32
	{
		None = 0,
		ReleaseHandle,
		EnableHandle,
		DisableHandle,
	};

	FSchedulePendingAction() = default;

	FSchedulePendingAction(EType InType, FScheduleHandle InHandle);

	FScheduleHandle Handle;

	EType Type = EType::None;
};

}

// Represents a scheduler interface to a UWorld
UCLASS()
class UAnimNextSchedulerWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend struct UE::AnimNext::FScheduler;

	// UWorldSubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	bool IsValidHandle(UE::AnimNext::FScheduleHandle InHandle) const;

	void FlushPendingActions();

	// Acquire a handle that binds a schedule with the supplied parameters
	UE::AnimNext::FScheduleHandle AcquireHandle(UObject* InObject, UAnimNextSchedule* InSchedule, EAnimNextScheduleInitMethod InInitMethod, TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>&& InInitializeCallback);

	// Release an already acquired handle
	// The full release of the binding referenced by the handle will be deferred after this call is made
	void ReleaseHandle(UE::AnimNext::FScheduleHandle InHandle);

	// Enables or disables the schedule parameterization represented by the supplied handle
	// This operation is deferred until the next time the schedule ticks
	void EnableHandle(UE::AnimNext::FScheduleHandle InHandle, bool bInEnabled);

	// Queue a task to run at a particular point in a schedule
	// @param	InHandle		The handle to queue the task to
	// @param	InTaskName		The name of the task in the schedule to run the supplied task relative to
	// @param	InTaskFunction	The function to run
	// @param	InLocation		Where to run the task, before or after
	void QueueTask(UE::AnimNext::FScheduleHandle InHandle, FName InScheduleTaskName, TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>&& InTaskFunction, UE::AnimNext::FScheduler::ETaskRunLocation InLocation);

#if WITH_EDITOR
	// Refresh any entries that use the provided schedule as it has been recompiled.
	void OnScheduleCompiled(UAnimNextSchedule* InSchedule);
#endif

	// Currently running entries, pooled
	TArray<TUniquePtr<FAnimNextSchedulerEntry>> Entries;

	// Free entries for the entries pool
	TArray<uint32> FreeEntryIndices;
	
	// Queued actions
	TArray<UE::AnimNext::FSchedulePendingAction> PendingActions;

	// Locks for concurrent modifications
	FRWLock EntriesLock;
	FRWLock PendingLock;

	// Cached delta time
	float DeltaTime;
};