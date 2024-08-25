// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Helpers/PCGAsyncState.h"

#include "Async/Async.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/MpscQueue.h"
#include "Tasks/Task.h"

#include <atomic>

struct FPCGContext;
struct FPCGPoint;

namespace FPCGAsync
{
	namespace ConsoleVar
	{
		extern PCG_API TAutoConsoleVariable<bool> CVarDisableAsyncTimeSlicing;
		extern PCG_API TAutoConsoleVariable<int32> CVarAsyncOverrideChunkSize;
	};

	/** 
	* Helper to do simple point processing loops
	* @param Context - The context containing the information about how many tasks to launch
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of points generated
	* @param OutPoints - The array in which the results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the index [0; NumIterations] and has to write to the point & return true when the current call generates a point
	*/
	PCG_API void AsyncPointProcessing(FPCGContext* Context, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<bool(int32, FPCGPoint&)>& PointFunc);
	
	/** 
	* Helper to do simple point processing loops
	* @param Context - The context containing the information about how many tasks to launch
	* @param InPoints - The array in which the source points will be read from
	* @param OutPoints - The array in which the results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the input point and has to write to the output point & return true when the current call generates a point
	*/
	PCG_API void AsyncPointProcessing(FPCGContext* Context, const TArray<FPCGPoint>& InPoints, TArray<FPCGPoint>& OutPoints, const TFunction<bool(const FPCGPoint&, FPCGPoint&)>& PointFunc);

	/** 
	* Helper to do more general 1:1 point processing loops
	* @param NumAvailableTasks - The upper bound on the number of async tasks we'll start
	* @param MinIterationsPerTask - The lower bound on the number of iterations per task we'll dispatch
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of points generated
	* @param OutPoints - The array in which the results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the index [0; NumIterations] and has to write to the point & return true when the current call generates a point
	*/
	void AsyncPointProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<bool(int32, FPCGPoint&)>& PointFunc);

	/**
	* Helper to do simple point filtering loops
	* @param Context - The context containing the information about how many tasks to launch
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of points generated
	* @param InFilterPoints - The array in which the in-filter results will be written to. Note that the array will be cleared before execution
	* @param OutFilterPoints - The array in which the out-filter results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the index [0; NumIterations] and has to write to the point & return true when the current call generates a point
	*/
	PCG_API void AsyncPointFilterProcessing(FPCGContext* Context, int32 NumIterations, TArray<FPCGPoint>& InFilterPoints, TArray<FPCGPoint>& OutFilterPoints, const TFunction<bool(int32, FPCGPoint&, FPCGPoint&)>& PointFunc);

	/**
	* Helper to do more general 1:1 point filtering loops
	* @param NumAvailableTasks - The upper bound on the number of async tasks we'll start
	* @param MinIterationsPerTask - The lower bound on the number of iterations per task we'll dispatch
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of points generated
	* @param InFilterPoints - The array in which the in-filter results will be written to. Note that the array will be cleared before execution
	* @param OutFilterPoints - The array in which the out-filter results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the index [0; NumIterations] and has to write to the point & return true when the current call generates a point
	*/
	void AsyncPointFilterProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& InFilterPoints, TArray<FPCGPoint>& OutFilterPoints, const TFunction<bool(int32, FPCGPoint&, FPCGPoint&)>& PointFunc);

	/**
	* Helper to do simple 1:N point processing loops
	* @param Context - The context containing the information about how many tasks to launch
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of points generated
	* @param OutPoints - The array in which the results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the index [0; NumIterations] and has to write to the point & return true when the current call generates a point
	*/
	PCG_API void AsyncMultiPointProcessing(FPCGContext* Context, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<TArray<FPCGPoint>(int32)>& PointFunc);

	/** 
	* Helper to do more general 1:N point processing loops
	* @param NumAvailableTasks - The upper bound on the number of async tasks we'll start
	* @param MinIterationsPerTask - The lower bound on the number of iterations per task we'll dispatch
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of points generated
 	* @param OutPoints - The array in which the results will be written to. Note that the array will be cleared before execution
	* @param PointFunc - A function that has the index [0; NumIterations] and has to write to the point & return true when the current call generates a point
	*/
	void AsyncMultiPointProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<TArray<FPCGPoint>(int32)>& PointFunc);

	namespace Private
	{
	template <typename InitializeFunc, typename InnerLoopFunc, typename MoveDataRangeFunc, typename FinishedFunc>
	bool AsyncProcessing(FPCGAsyncState& AsyncState, int32 NumIterations, InitializeFunc&& Initialize, InnerLoopFunc&& IterationInnerLoop, MoveDataRangeFunc&& MoveDataRange, FinishedFunc&& Finished, const bool bInEnableTimeSlicing, const int32 InChunkSize)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::AsyncProcessing);

		const int32 OverrideChunkSize = ConsoleVar::CVarAsyncOverrideChunkSize.GetValueOnAnyThread();
		const int32 ChunkSize = OverrideChunkSize > 0 ? OverrideChunkSize : InChunkSize;

		const int32 StartIndex = AsyncState.AsyncCurrentReadIndex;
		const bool bEnableTimeSlicing = bInEnableTimeSlicing && !ConsoleVar::CVarDisableAsyncTimeSlicing.GetValueOnAnyThread();

		if (AsyncState.NumAvailableTasks <= 0 || ChunkSize <= 0 || NumIterations <= 0 || StartIndex < 0)
		{
			// Invalid request
			return true;
		}

		if (StartIndex >= NumIterations)
		{
			// Nothing left to do
			return true;
		}

		const int32 RemainingIterations = NumIterations - StartIndex;
		const int32 ChunksNumber = 1 + ((RemainingIterations - 1) / ChunkSize);
		const int32 NumTasks = FMath::Min(AsyncState.NumAvailableTasks, ChunksNumber);
		const int32 NumFutures = NumTasks - 1;

		if (StartIndex == 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::Private::AsyncProcessing::Initialize);

			Initialize();
		}

		// Synchronisation structure to be shared between async tasks and collapsing main thread.
		struct FSynchroStruct 
		{
			// Atomic counter to know which chunk a given task should process.
			std::atomic<int32> CurrentChunkToProcess = 0;

			// Atomic to indicate tasks to stop processing new chunks.
			std::atomic<bool> bQuit = false;

			// Queue for worker to indicate the current chunk that was processed and the number of elements written.
			// Threadsafe for MPSC: Multiple Producers (async tasks) and Single Consumer (collapsing task).
			TMpscQueue<TPair<int32, int32>> ChunkProcessedIndexAndNumElementsWrittenQueue;
		};

		// We won't stop if we are not time slicing.
		auto ShouldStop = [AsyncState, bEnableTimeSlicing]() -> bool
		{
			return bEnableTimeSlicing && AsyncState.ShouldStop();
		};

		// Main thread will either work if there is no future, or collapse arrays if there are some.
		if (NumFutures == 0)
		{
			// Main thread is working
			int32 CurrentChunkToProcess = 0;
			check(ChunksNumber > 0);

			// Do at least one run, as we could fall into an infinite loop if we always have to stop before doing anything.
			do
			{
				const int32 StartReadIndex = AsyncState.AsyncCurrentReadIndex;
				const int32 StartWriteIndex = AsyncState.AsyncCurrentWriteIndex;
				const int32 Count = FMath::Min(ChunkSize, NumIterations - StartReadIndex);

				check(Count > 0);

				int32 NumElementsWritten;

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::Private::AsyncProcessing::IterationInnerLoop);

					NumElementsWritten = IterationInnerLoop(StartReadIndex, StartWriteIndex, Count);
				}

				AsyncState.AsyncCurrentWriteIndex += NumElementsWritten;
				AsyncState.AsyncCurrentReadIndex += ChunkSize;
				++CurrentChunkToProcess;
			} while (CurrentChunkToProcess < ChunksNumber && !ShouldStop());
		}
		else
		{
			// Main thread is collapsing
			// First start the futures
			FSynchroStruct SynchroStruct{};

			// Futures are not returning anything.
			TArray<UE::Tasks::TTask<void>> AsyncTasks;
			AsyncTasks.Reserve(NumFutures);

			auto JobTask = [&IterationInnerLoop, &SynchroStruct, ChunksNumber, StartIndex, ChunkSize, NumIterations]()
			{
				const int32 CurrentChunkToProcess = SynchroStruct.CurrentChunkToProcess++;
				if (CurrentChunkToProcess >= ChunksNumber)
				{
					// If we reached the end, notify eveyone it's done
					SynchroStruct.bQuit = true;
					return;
				}

				const int32 StartReadIndex = StartIndex + CurrentChunkToProcess * ChunkSize;
				// We write in the same "memory space" with the async task (in preallocated output array) and will be collapsed by the main thread.
				const int32 StartWriteIndex = StartReadIndex;
				const int32 Count = FMath::Min(ChunkSize, NumIterations - StartReadIndex);

				check(Count > 0);

				int32 NumElementsWritten;

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::Private::AsyncProcessing::InnerLoop);

					NumElementsWritten = IterationInnerLoop(StartReadIndex, StartWriteIndex, Count);
				}

				SynchroStruct.ChunkProcessedIndexAndNumElementsWrittenQueue.Enqueue(CurrentChunkToProcess, NumElementsWritten);
			};

			for (int32 TaskIndex = 0; TaskIndex < NumFutures; ++TaskIndex)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::Private::AsyncProcessing::StartingTasks);
				AsyncTasks.Emplace(UE::Tasks::Launch(UE_SOURCE_LOCATION, [&JobTask, &SynchroStruct]() -> void
				{
					// Do at least one run, as we could fall into an infinite loop if we always have to stop before doing anything.
					do
					{
						JobTask();
					} while (!SynchroStruct.bQuit);
				}));
			}

			// Collapsing needs to be done in order, so we have a map between chunk index and the number of elements written for this chunk.
			// Note that we need to collpase because points can be discarded, so we can have less points "kept" than the chunk size.
			TMap<int32, int32> ChunkToNumElementsWrittenMap;
			bool bLocalStop = false;
			int32 CurrentChunkToCollapse = 0;
			while (true)
			{
				// Either we should stop because the time has elapsed or all workloads have been dispatched
				if (!bLocalStop && (ShouldStop() || SynchroStruct.bQuit))
				{
					SynchroStruct.bQuit = true;
					bLocalStop = true;

					// Wait for all futures to finish their job
					UE::Tasks::Wait(AsyncTasks);
				}

				TPair<int32, int32> QueueItem;
				// Try to unqueue a processed item.
				if (!SynchroStruct.ChunkProcessedIndexAndNumElementsWrittenQueue.Dequeue(QueueItem) && !bLocalStop)
				{
					// If there is nothing to unqueue (nothing to collapse), we take this wait opportunity to
					// process a chunk on the main thread.
					JobTask();
					continue;
				}
				else
				{
					// If we successfully unqueue, it means we can try to collapse.
					TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::AsyncProcessing::CollapsingNewData);

					ChunkToNumElementsWrittenMap.Add(QueueItem);

					// We always collapse in order, so try to collapse chunk in order while we have some in the map.
					int32 NumberOfElementsWritten = 0;
					
					while (ChunkToNumElementsWrittenMap.RemoveAndCopyValue(CurrentChunkToCollapse, NumberOfElementsWritten))
					{
						const int32 ReadStartIndexForCurrentChunkToCollapse = AsyncState.AsyncCurrentReadIndex;
						check(NumberOfElementsWritten <= ChunkSize);

						// If ReadStartIndexForCurrentChunkToCollapse == AsyncState.AsyncCurrentWriteIndex, no need to copy, all elements
						// are already at the right place
						if (ReadStartIndexForCurrentChunkToCollapse == AsyncState.AsyncCurrentWriteIndex)
						{
							AsyncState.AsyncCurrentWriteIndex += NumberOfElementsWritten;
						}
						else
						{
							// Otherwise collapse
							MoveDataRange(ReadStartIndexForCurrentChunkToCollapse, AsyncState.AsyncCurrentWriteIndex, NumberOfElementsWritten);
							AsyncState.AsyncCurrentWriteIndex += NumberOfElementsWritten;
						}

						AsyncState.AsyncCurrentReadIndex += ChunkSize;
						++CurrentChunkToCollapse;
					}
				}

				// If we locally stoped (meaning we already wait for async tasks to finish) and we collapse all chunks processed
				// we can exit the loop
				if (bLocalStop && CurrentChunkToCollapse == FMath::Min((int32)SynchroStruct.CurrentChunkToProcess, ChunksNumber))
				{
					break;
				}
			}
		}

		const bool bIsDone = (AsyncState.AsyncCurrentReadIndex >= NumIterations);

		check(bIsDone || bEnableTimeSlicing);

		if (bIsDone)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAsync::Private::AsyncProcessing::Finished);

			Finished(AsyncState.AsyncCurrentWriteIndex);

			// Reset indexes
			AsyncState.AsyncCurrentReadIndex = 0;
			AsyncState.AsyncCurrentWriteIndex = 0;
		}

		return bIsDone;
	}
	}

	/**
	* A Helper for generic parallel loops, with support for timeslicing, specialized to work on ranges. This version only uses indexes allowing more flexible usage
	* to process many arrays in parallel or use it for other batch updating.
	* 
	* Work will be separated in chunks, that will be processed in parallel. Main thread will then collapse incoming data from async tasks.
	* Will use AsyncState.ShouldStop() to stop execution if timeslicing is enabled.
	* Important info: 
	*   - We will finish to process and collapse data for all data already in process, even if we need to stop. To mitigate this, try to use small chunk sizes.
	*   - To avoid infinite loops (when we should stop even before starting working), we will at least process 1 chunk of data per thread.
	*   - To have async tasks, you need to have at least 3 available threads (main thread + 2 futures). Otherwise, we will only process on the main thread, without collapse.
	* 
	* @param AsyncState - The context containing the information about how many tasks we can launch, async read/write index for the current job and a function to know if we need to stop processing.
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	* @param Initialize - Signature: void(). A function that will be called once on the first timeslice, where you can reserve data for processing
	* @param ProcessRange - Signature: int32(int32 StartReadIndex, int32 StartWriteIndex, int32 Count). A function that processes a range of values and returns the number of written values 
	* @param MoveDataRange - Signature: void(int32 ReadIndex, int32 WriteIndex, int32 Count). If the processing filters points, this will be used to move elements in chunk from one range to another 
	* @param Finished - Signature: void(int32 Count). Called once on finished, and tells you the total count of points written.
	* @param bEnableTimeSlicing - If false, we will not stop until all the processing is done.
	* @param ChunkSize - Size of the chunks to cut the input data with
	* @returns true if the processing is done, false otherwise. Use this to know if you need to reschedule the task.
	*/
	template <typename InitializeFunc, typename ProcessRangeFunc, typename MoveDataRangeFunc, typename FinishedFunc>
	bool AsyncProcessingRangeEx(FPCGAsyncState* AsyncState, int32 NumIterations, InitializeFunc&& Initialize, ProcessRangeFunc&& ProcessRange, MoveDataRangeFunc&& MoveDataRange, FinishedFunc&& Finished, const bool bEnableTimeSlicing, const int32 ChunkSize = 64)
	{
		if (AsyncState && !AsyncState->bIsRunningAsyncCall)
		{
			AsyncState->bIsRunningAsyncCall = true;
			const bool bIsDone = Private::AsyncProcessing(*AsyncState, NumIterations, Initialize, ProcessRange, MoveDataRange, Finished, bEnableTimeSlicing, ChunkSize);
			AsyncState->bIsRunningAsyncCall = false;
			return bIsDone;
		}
		else
		{
			// Can't use time slicing without an async state or while running in another async call (it will mess up with async indexes). 
			// We also force using one thread (the current one).
			FPCGAsyncState DummyState;
			DummyState.NumAvailableTasks = 1;
			DummyState.bIsRunningAsyncCall = true;
			return Private::AsyncProcessing(DummyState, NumIterations, Initialize, ProcessRange, MoveDataRange, Finished, /*bEnableTimeSlicing=*/false, ChunkSize);
		}
	}

	/**
	* A Helper for generic parallel loops, with support for timeslicing. This version only uses indexes allowing more flexible usage
	* to process many arrays in parallel or use it for other batch updating.
	* 
	* Work will be separated in chunks, that will be processed in parallel. Main thread will then collapse incoming data from async tasks.
	* Will use AsyncState.ShouldStop() to stop execution if timeslicing is enabled.
	* Important info: 
	*   - We will finish to process and collapse data for all data already in process, even if we need to stop. To mitigate this, try to use small chunk sizes.
	*   - To avoid infinite loops (when we should stop even before starting working), we will at least process 1 chunk of data per thread.
	*   - To have async tasks, you need to have at least 3 available threads (main thread + 2 futures). Otherwise, we will only process on the main thread, without collapse.
	* 
	* @param AsyncState - The context containing the information about how many tasks we can launch, async read/write index for the current job and a function to know if we need to stop processing.
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	* @param Initialize - Signature: void(). A function that will be called once on the first timeslice, where you can reserve data for processing
	* @param ProcessElement - Signature: bool(int32 ReadIndex, int32 WriteIndex). A function that processes  
	* @param MoveData - Signature: void(int32 ReadIndex, int32 WriteIndex). If the processing filters points, this will be used to move elements from one index to another 
	* @param Finished - Signature: void(int32 Count). Called once on finished, and tells you the total count of points written.
	* @param bEnableTimeSlicing - If false, we will not stop until all the processing is done.
	* @param ChunkSize - Size of the chunks to cut the input data with
	* @returns true if the processing is done, false otherwise. Use this to know if you need to reschedule the task.
	*/
	template <typename InitializeFunc, typename ProcessElementFunc, typename MoveFunc, typename FinishedFunc>
	bool AsyncProcessingEx(FPCGAsyncState* AsyncState, int32 NumIterations, InitializeFunc&& Initialize, ProcessElementFunc&& ProcessElement, MoveFunc&& MoveData, FinishedFunc&& Finished, const bool bEnableTimeSlicing, const int32 ChunkSize = 64)
	{
		auto IterationInnerLoop = [Func = MoveTemp(ProcessElement)](int32 StartReadIndex, int32 StartWriteIndex, int32 Count) -> int32
		{
			int32 NumPointsWritten = 0;

			for (int32 i = 0; i < Count; ++i)
			{
				if (Func(StartReadIndex + i, StartWriteIndex + NumPointsWritten))
				{
					++NumPointsWritten;
				}
			}

			return NumPointsWritten;
		};

		auto MoveDataRange = [Func = MoveTemp(MoveData)](int32 ReadIndex, int32 WriteIndex, int32 Count)
		{
			for (int Index = 0; Index < Count; ++Index)
			{
				Func(ReadIndex + Index, WriteIndex + Index);
			}
		};

		return AsyncProcessingRangeEx(AsyncState, NumIterations, Initialize, IterationInnerLoop, MoveDataRange, Finished, bEnableTimeSlicing, ChunkSize);
	}
 
 	/**
	* A Helper for generic parallel loops, with support for timeslicing. This version only uses indexes allowing more flexible usage
	* to process any data format.
	* 
	* This version also does not support removing elements and is expects a 1:1 mapping. This just removes some lambdas required to support
	* that use case.
	* 
	* Work will be separated in chunks, that will be processed in parallel. Main thread will then collapse incoming data from async tasks.
	* Will use AsyncState.ShouldStop() to stop execution if timeslicing is enabled.
	* Important info: 
	*   - We will finish to process and collapse data for all data already in process, even if we need to stop. To mitigate this, try to use small chunk sizes.
	*   - To avoid infinite loops (when we should stop even before starting working), we will at least process 1 chunk of data per thread.
	*   - To have async tasks, you need to have at least 3 available threads (main thread + 2 futures). Otherwise, we will only process on the main thread, without collapse.
	* 
	* @param AsyncState - The context containing the information about how many tasks we can launch, async read/write index for the current job and a function to know if we need to stop processing.
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	* @param Initialize - Signature: void(). A function that will be called once on the first timeslice, where you can reserve data.
	* @param ProcessElement - Signature: bool(int32 ReadIndex, int32 WriteIndex). A function that processes  
	* @param bEnableTimeSlicing - If false, we will not stop until all the processing is done.
	* @param ChunkSize - Size of the chunks to cut the input data with
	* @returns true if the processing is done, false otherwise. Use this to know if you need to reschedule the task.
	*/
 	template <typename InitializeFunc, typename ProcessElementFunc>
	bool AsyncProcessingOneToOneEx(FPCGAsyncState* AsyncState, int32 NumIterations, InitializeFunc&& Initialize, ProcessElementFunc&& ProcessElement, const bool bEnableTimeSlicing, const int32 ChunkSize = 64)
	{
		auto IterationInnerLoop = [Func = MoveTemp(ProcessElement)](int32 StartReadIndex, int32 StartWriteIndex, int32 Count) -> int32
		{
			for (int32 i = 0; i < Count; ++i)
			{
				Func(StartReadIndex + i, StartWriteIndex + i);
			}

			return Count;
		};

		auto MoveDataRange = [](int32, int32, int32) { ensure(false); };
		auto Finished = [NumIterations](int32 Count) { ensure(NumIterations == Count); };

		if (AsyncState && !AsyncState->bIsRunningAsyncCall)
		{
			AsyncState->bIsRunningAsyncCall = true;
			const bool bIsDone = Private::AsyncProcessing(*AsyncState, NumIterations, Initialize, IterationInnerLoop, MoveDataRange, Finished, bEnableTimeSlicing, ChunkSize);
			AsyncState->bIsRunningAsyncCall = false;
			return bIsDone;
		}
		else
		{
			// Can't use time slicing without an async state or while running in another async call (it will mess up with async indexes). 
			// We also force using one thread (the current one).
			FPCGAsyncState DummyState;
			DummyState.NumAvailableTasks = 1;
			DummyState.bIsRunningAsyncCall = true;
			return Private::AsyncProcessing(DummyState, NumIterations, Initialize, IterationInnerLoop, MoveDataRange, Finished, /*bEnableTimeSlicing=*/false, ChunkSize);
		}
	}

	/**
	* Helper for generic parallel loops, with support for timeslicing. This version takes a data type, expecting processing on an
	* array of that type.
	* 
	* Work will be separated in chunks, that will be processed in parallel. Main thread will then collapse incoming data from async tasks.
	* Will use AsyncState.ShouldStop() to stop execution if timeslicing is enabled.
	* Important info: 
	*   - We will finish to process and collapse data for all data already in process, even if we need to stop. To mitigate this, try to use small chunk sizes.
	*   - To avoid infinite loops (when we should stop even before starting working), we will at least process 1 chunk of data per thread.
	*   - To have async tasks, you need to have at least 3 available threads (main thread + 2 futures). Otherwise, we will only process on the main thread, without collapse.
	* 
	* @param AsyncState - The context containing the information about how many tasks we can launch, async read/write index for the current job and a function to know if we need to stop processing.
	* @param NumIterations - The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	* @param OutData - The array in which the results will be written to. Note that the array will be cleared before execution.
	* @param Func - Signature: bool(int32, OutputType&). A function that has the index [0; NumIterations] and has to write to some data & return false if the result should be discarded. 
	* @param bEnableTimeSlicing - If false, we will not stop until all the processing is done.
	* @param ChunkSize - Size of the chunks to cut the input data with
	* @returns true if the processing is done, false otherwise. Use this to know if you need to reschedule the task.
	*/
	template <typename OutputType, typename Func>
	bool AsyncProcessing(FPCGAsyncState* AsyncState, int32 NumIterations, TArray<OutputType>& OutData, Func&& InFunc, const bool bEnableTimeSlicing, const int32 ChunkSize = 64)
	{
		auto Initialize = [&OutData, NumIterations]()
		{
			OutData.SetNumUninitialized(NumIterations);
		};
		
		auto IterationInnerLoop = [Func = MoveTemp(InFunc), &OutData](int32 ReadIndex, int32 WriteIndex) -> int32
		{
			return Func(ReadIndex, OutData[WriteIndex]);
		};

		auto MoveData = [&OutData](int32 ReadIndex, int32 WriteIndex)
		{
			OutData[WriteIndex] = std::move(OutData[ReadIndex]);
		};

		auto Finished = [&OutData](int32 Count)
		{
			// Shrinking can have a big impact on the performance, but without it, we can also hold a big chunk of wasted memory.
			// Might revisit later if the performance impact is too big.
			OutData.SetNum(Count);
		};

		return AsyncProcessingEx(AsyncState, NumIterations, Initialize, IterationInnerLoop, MoveData, Finished, bEnableTimeSlicing, ChunkSize);
	}
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
