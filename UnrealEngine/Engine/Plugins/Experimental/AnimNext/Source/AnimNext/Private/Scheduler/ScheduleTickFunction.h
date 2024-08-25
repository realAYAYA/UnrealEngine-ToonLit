// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Containers/SpscQueue.h"

struct FAnimNextScheduleInstruction;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FParamStackLayerHandle;
}

namespace UE::AnimNext
{

struct FScheduleBeginTickFunction : public FTickFunction
{
	FScheduleBeginTickFunction(FAnimNextSchedulerEntry& InEntry)
		: Entry(InEntry)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	void Run(float DeltaTime);

	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PreExecuteTasks;
	FAnimNextSchedulerEntry& Entry;
	FTickPrerequisite Subsequent;
};

struct FScheduleEndTickFunction : public FTickFunction
{
	FScheduleEndTickFunction(FAnimNextSchedulerEntry& InEntry)
		: Entry(InEntry)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	void Run();

	FAnimNextSchedulerEntry& Entry;
};

struct FScheduleTickFunction : public FTickFunction
{
	FScheduleTickFunction(const FScheduleContext& InScheduleContext, TConstArrayView<FAnimNextScheduleInstruction> InInstructions, TConstArrayView<TWeakObjectPtr<UObject>> InTargetObjects)
		: ScheduleContext(InScheduleContext)
		, Instructions(InInstructions)
		, TargetObjects(InTargetObjects)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	void Run();

	// Called standalone to run a whole schedule in one call
	static void RunSchedule(const FAnimNextSchedulerEntry& InEntry);

	// Static helper for running a slice of instructions
	static void RunScheduleHelper(const FScheduleContext& InScheduleContext, TConstArrayView<FAnimNextScheduleInstruction> InInstructions, TConstArrayView<TWeakObjectPtr<UObject>> InTargetObjects, TFunctionRef<void(void)> InPreExecuteScope, TFunctionRef<void(void)> InPostExecuteScope);

	const FScheduleContext& ScheduleContext;
	TConstArrayView<FAnimNextScheduleInstruction> Instructions;
	TArray<FTickPrerequisite> Subsequents;
	TConstArrayView<TWeakObjectPtr<UObject>> TargetObjects;
	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PreExecuteTasks;
	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PostExecuteTasks;
};

}