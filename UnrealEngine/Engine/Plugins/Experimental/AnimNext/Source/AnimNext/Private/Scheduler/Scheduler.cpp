// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulePortAdapterContext.h"
#include "Scheduler/SchedulePortDefinition.h"
#include "Scheduler/ScheduleContext.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"
#include "Tasks/Task.h"
#include "UObject/GCObject.h"
#include "LODPose.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectIterator.h"

namespace UE::AnimNext
{

struct FSchedulerImpl
{
	FDelegateHandle OnWorldPreActorTickHandle;

	TMap<FName, FSchedulePortDefinition> RegisteredPortDefinitions;

	uint32 SerialNumber = 0;
};

static FSchedulerImpl Impl;

void FScheduler::Init()
{
	// Kick off root task at the start of each world tick
	Impl.OnWorldPreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddLambda([](UWorld* InWorld, ELevelTick InTickType, float InDeltaSeconds)
	{
		if (InTickType == LEVELTICK_All || InTickType == LEVELTICK_ViewportsOnly)
		{
			if(UAnimNextSchedulerWorldSubsystem* Subsystem = InWorld->GetSubsystem<UAnimNextSchedulerWorldSubsystem>())
			{
				// Flush actions here as they require game thread callbacks (e.g. to reconfigure tick functions)
				Subsystem->FlushPendingActions();
				Subsystem->DeltaTime = InDeltaSeconds;
			}
		}
	});
}

void FScheduler::Destroy()
{
	FWorldDelegates::OnWorldPreActorTick.Remove(Impl.OnWorldPreActorTickHandle);
}

FScheduleHandle FScheduler::AcquireHandle(UObject* InObject, UAnimNextSchedule* InSchedule, EAnimNextScheduleInitMethod InInitMethod, TUniqueFunction<void(const FScheduleContext&)>&& InInitializeCallback)
{
	FScheduleHandle Handle;

	// Check parameters
	if (InSchedule == nullptr)
	{
		UE_LOG(LogAnimation, Warning, TEXT("FScheduler::AcquireHandle: Invalid schedule"));
		return FScheduleHandle();
	}

	if (InObject == nullptr)
	{
		UE_LOG(LogAnimation, Warning, TEXT("FScheduler::AcquireHandle: Invalid object"));
		return FScheduleHandle();
	}

	UWorld* World = InObject->GetWorld();
	if(World == nullptr)
	{
		return FScheduleHandle();
	}

	UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
	if(Subsystem == nullptr)
	{
		return FScheduleHandle();
	}

	return Subsystem->AcquireHandle(InObject, InSchedule, InInitMethod, MoveTemp(InInitializeCallback));
}

void FScheduler::ReleaseHandle(UObject* InObject, FScheduleHandle& InHandle)
{
	if(!InHandle.IsValid())
	{
		return;
	}
	
	UWorld* World = InObject->GetWorld();
	if(World == nullptr)
	{
		return;
	}

	UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
	if(Subsystem == nullptr)
	{
		return;
	}

	Subsystem->ReleaseHandle(InHandle);
	InHandle.Invalidate();
}

void FScheduler::EnableHandle(UObject* InObject, FScheduleHandle InHandle, bool bInEnabled)
{
	if(!InHandle.IsValid())
	{
		return;
	}
	
	UWorld* World = InObject->GetWorld();
	if(World == nullptr)
	{
		return;
	}

	UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
	if(Subsystem == nullptr)
	{
		return;
	}

	Subsystem->EnableHandle(InHandle, bInEnabled);
}

void FScheduler::QueueTask(UObject* InObject, FScheduleHandle InHandle, FName InScheduleTaskName, TUniqueFunction<void(const FScheduleContext&)>&& InTaskFunction, ETaskRunLocation InLocation)
{
	if(!InHandle.IsValid())
	{
		return;
	}
	
	UWorld* World = InObject->GetWorld();
	if(World == nullptr)
	{
		return;
	}

	UAnimNextSchedulerWorldSubsystem* Subsystem = World->GetSubsystem<UAnimNextSchedulerWorldSubsystem>();
	if(Subsystem == nullptr)
	{
		return;
	}

	Subsystem->QueueTask(InHandle, InScheduleTaskName, MoveTemp(InTaskFunction), InLocation);
}

#if WITH_EDITOR

void FScheduler::OnScheduleCompiled(UAnimNextSchedule* InSchedule)
{
	for(TObjectIterator<UAnimNextSchedulerWorldSubsystem> It; It; ++It)
	{
		It->OnScheduleCompiled(InSchedule);
	}
}

#endif

}