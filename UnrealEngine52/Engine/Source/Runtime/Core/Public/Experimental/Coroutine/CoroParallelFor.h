// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Coroutine.h"

template<typename BodyType, typename... ARGS>
inline void CoroParallelFor(const TCHAR* DebugName, int32 Num, BodyType Body, EParallelForFlags Flags, ARGS&... Args)
{
	int32 NumWorkers = 1;
	const bool bIsMultithread = FApp::ShouldUseThreadingForPerformance() || FForkProcessHelper::IsForkedMultithreadInstance();
	if (Num > 1 && (Flags & EParallelForFlags::ForceSingleThread) == EParallelForFlags::None && bIsMultithread)
	{
		NumWorkers = int32(LowLevelTasks::FScheduler::Get().GetNumWorkers());
		NumWorkers = LowLevelTasks::FScheduler::Get().IsWorkerThread() ? NumWorkers : NumWorkers + 1; //named threads help with the work
		NumWorkers = FMath::Min3(NumWorkers, Num, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	}

	//single threaded mode
	if (NumWorkers <= 1)
	{
		// no threads, just do it and return
		for(int32 Index = 0; Index < Num; Index++)
		{
			SYNC_INVOKE(Body(Index, Args...));
		}
		return;
	}
	checkSlow(NumWorkers > 0);

	int32 BatchSize = 1;
	int32 NumBatches = Num;
	bool bIsUnbalanced = (Flags & EParallelForFlags::Unbalanced) == EParallelForFlags::Unbalanced;
	if (!bIsUnbalanced)
	{
		for (int32 Div = 6; Div; Div--)
		{
			if (Num >= (NumWorkers * Div))
			{	
				BatchSize = FMath::DivideAndRoundUp<int32>(Num, (NumWorkers * Div));
				NumBatches = FMath::DivideAndRoundUp<int32>(Num, BatchSize);

				if (NumBatches >= NumWorkers)
				{
					break;
				}
			}
		}
	}
	checkSlow(BatchSize * NumBatches >= Num);

	std::atomic_int BatchItem = {0};
	auto DoWorkLambda = [](std::atomic_int& InnerBatchItem, BodyType InnerBody, int32 InnerBatchSize, int32 InnerNum, auto&... InnerArgs) -> CORO_TASK(void)
	{
		FMemMark Mark(FMemStack::Get());
		TRACE_CPUPROFILER_EVENT_SCOPE(CoroTask_ParallelFor);
		while(true)
		{
			int32 StartIndex = InnerBatchItem.fetch_add(InnerBatchSize, std::memory_order_relaxed);
			int32 EndIndex = FMath::Min<int32>(StartIndex + InnerBatchSize, InnerNum);
			for (int32 Index = StartIndex; Index < EndIndex; Index++)
			{
				CORO_INVOKE(InnerBody(Index, InnerArgs...));
			}

			if (EndIndex >= InnerNum)
			{
				break;
			}
		}

		CO_RETURN_TASK();
	};

	//Try to inherit the Priority from the caller
	// Anything scheduled by the task graph is latency sensitive because it might impact the frame rate. Anything else is not (i.e. Worker / Background threads).
	const ETaskTag LatencySensitiveTasks = 
		ETaskTag::EStaticInit | 
		ETaskTag::EGameThread | 
		ETaskTag::ESlateThread | 
		ETaskTag::ERenderingThread | 
		ETaskTag::ERhiThread;

	const bool bBackgroundPriority = (Flags & EParallelForFlags::BackgroundPriority) != EParallelForFlags::None;
	const bool bIsLatencySensitive = (FTaskTagScope::GetCurrentTag() & LatencySensitiveTasks) != ETaskTag::ENone;

	LowLevelTasks::ETaskPriority Priority = LowLevelTasks::ETaskPriority::Inherit;
	if (bIsLatencySensitive && !bBackgroundPriority)
	{
		Priority =  LowLevelTasks::ETaskPriority::High;
	}
	else if (bBackgroundPriority)
	{
		Priority = LowLevelTasks::ETaskPriority::BackgroundNormal;
	}

	//add extra worker contexts for suspended tasks
	NumWorkers *= 2;

	TArray<LAUNCHED_TASK(void), TInlineAllocator<16>> WorkerTasks;
	WorkerTasks.Reserve(NumWorkers);
	for (int32 Worker = 0; Worker < NumWorkers; Worker++)
	{
		WorkerTasks.Push(DoWorkLambda(BatchItem, Body, BatchSize, Num, Args...).Launch(DebugName, Priority, LowLevelTasks::EQueuePreference::GlobalQueuePreference));
	}

	int32 TaskIndex = WorkerTasks.Num() - 1;
	while (WorkerTasks.Num())
	{
		if (TaskIndex < 0)
		{
			TaskIndex = WorkerTasks.Num() - 1;
		}
		if (WorkerTasks[TaskIndex].TryExpedite())//LowLevelTasks::ECancellationFlags::None))
		{
			WorkerTasks.RemoveAt(TaskIndex);
		}
		TaskIndex--;
	}
	checkSlow(BatchItem.load(std::memory_order_relaxed) * BatchSize >= Num);
}