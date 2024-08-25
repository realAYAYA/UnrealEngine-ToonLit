// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "CoreTypes.h"
#include <cfloat>
#include "Misc/QueuedThreadPool.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "TraceServices/Common/CancellationToken.h"
#include "TraceServices/Containers/Timelines.h"

namespace TraceServices
{

struct FAggregatedTimingStats
{
	uint64 InstanceCount = 0;
	double TotalInclusiveTime = 0.0;
	double MinInclusiveTime = DBL_MAX;
	double MaxInclusiveTime = -DBL_MAX;
	double AverageInclusiveTime = 0.0;
	double MedianInclusiveTime = 0.0;
	double TotalExclusiveTime = 0.0;
	double MinExclusiveTime = DBL_MAX;
	double MaxExclusiveTime = -DBL_MAX;
	double AverageExclusiveTime = 0.0;
	double MedianExclusiveTime = 0.0;
};

struct FFrameData
{
	double StartTime;
	double EndTime;
};

class FTimelineStatistics
{
public:
	template<typename TimelineType, typename BucketMappingFunc, typename BucketKeyType>
	static void CreateAggregation(const TArray<const TimelineType*>& Timelines,
								  BucketMappingFunc BucketMapper,
								  double IntervalStart,
								  double IntervalEnd,
								  TSharedPtr<FCancellationToken> CancellationToken,
								  TMap<BucketKeyType, FAggregatedTimingStats>& Result)
	{
		TMap<BucketKeyType, FInternalAggregationEntry> InternalResult;
		// Compute instance count and total/min/max inclusive/exclusive times for each timer.
		for (const TimelineType* Timeline : Timelines)
		{
			if (CancellationToken.IsValid() && CancellationToken->ShouldCancel())
			{
				return;
			}
			ProcessTimeline(Timeline, BucketMapper, UpdateTotalMinMaxTimerAggregationStats, IntervalStart, IntervalEnd, InternalResult);
		}
		// Now, as we know min/max inclusive/exclusive times for each timer, we can compute histogram and median values.
		const bool ComputeMedian = true;
		if (ComputeMedian)
		{
			// Update bucket size (DT) for computing histogram.
			for (auto& KV : InternalResult)
			{
				PreComputeHistogram(KV.Value);
			}

			// Compute histogram.
			for (const TimelineType* Timeline : Timelines)
			{
				if (CancellationToken.IsValid() && CancellationToken->ShouldCancel())
				{
					return;
				}

				ProcessTimeline(Timeline, BucketMapper, UpdateHistogramForTimerStats, IntervalStart, IntervalEnd, InternalResult);
			}
		}

		// Compute average and median inclusive/exclusive times.
		for (auto& KV : InternalResult)
		{
			PostProcessTimerStats(KV.Value, ComputeMedian);
			Result.Add(KV.Key, KV.Value.Stats);
		}
	};

	template<typename TimelineType, typename BucketMappingFunc, typename BucketKeyType>
	static void CreateFrameStatsAggregation(const TArray<const TimelineType*>& Timelines,
											BucketMappingFunc BucketMapper,
											const TArray<FFrameData>& Frames,
											TSharedPtr<FCancellationToken> CancellationToken,
											TMap<BucketKeyType, FAggregatedTimingStats>& Result)
	{
		TMap<BucketKeyType, FInternalFrameAggregationEntry> GlobalResult;
		TMap<BucketKeyType, FAggregatedTimingStats> InitialTimerMap;
		double StartTime = Frames[0].StartTime;
		double EndTime = Frames[Frames.Num() - 1].EndTime;

		// Gather the keys for all timers so we have a stable map (no insertions necessary) for the next step.
		// This is to guarantee that GlobalResult and FrameResult have the same iteration order.
		for (const TimelineType* Timeline : Timelines)
		{
			GatherKeysFromTimeline(Timeline, BucketMapper, StartTime, EndTime, InitialTimerMap);
		}

		int32 FramesNum = Frames.Num();

		// For very large numbers of timers or frames, an out of memory is possible so don't compute the median.
		constexpr int64 MaxSize = 5 * 100 * 1000 * 1000;
		bool bComputeMedian = InitialTimerMap.Num() * (int64)FramesNum < MaxSize;
		for (auto& KV : InitialTimerMap)
		{
			FInternalFrameAggregationEntry Entry;
			if (bComputeMedian)
			{
				Entry.FrameInclusiveTimes.AddUninitialized(Frames.Num());
				Entry.FrameExclusiveTimes.AddUninitialized(Frames.Num());
			}
			Entry.Inner.Stats = KV.Value;
			GlobalResult.Add(KV.Key, MoveTemp(Entry));
		}

		TQueue<TSharedPtr<TMap<BucketKeyType, FAggregatedTimingStats>>, EQueueMode::Mpsc> FrameResultsQueue;

		constexpr float RatioOfThreadsToUse = 0.75f;
		int32 NumTasks = FMath::Max(1, (int32)((float)GThreadPool->GetNumThreads() * RatioOfThreadsToUse));
		NumTasks = FMath::Min((int32)NumTasks, Frames.Num());
		int32 FramesPerTask = Frames.Num() / NumTasks;
		int32 ExtraFrameTasks = Frames.Num() - NumTasks * FramesPerTask;

		int32 StartIndex = 0;
		int32 EndIndex = FramesPerTask + (ExtraFrameTasks > 0 ? 1 : 0);
		--ExtraFrameTasks;

		TArray<UE::Tasks::TTask<void>> AsyncTasks;
		for (int32 Index = 0; Index < NumTasks; ++Index)
		{
			AsyncTasks.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [StartIndex, EndIndex, &Timelines, bComputeMedian, BucketMapper, &Frames, &FrameResultsQueue, &InitialTimerMap, &GlobalResult, CancellationToken]()
			{
				if (CancellationToken.IsValid() && CancellationToken->ShouldCancel())
				{
					return;
				}

				TSharedPtr<TMap<BucketKeyType, FAggregatedTimingStats>> TaskResult = MakeShared<TMap<BucketKeyType, FAggregatedTimingStats>>(InitialTimerMap);
				TMap<BucketKeyType, FAggregatedTimingStats> FrameResult(InitialTimerMap);

				for(int FrameIndex = StartIndex; FrameIndex < EndIndex; ++FrameIndex)
				{
					if (CancellationToken.IsValid() && CancellationToken->ShouldCancel())
					{
						return;
					}

					// Compute instance count and total/min/max inclusive/exclusive times for each timer.
					for (const TimelineType* Timeline : Timelines)
					{
						ProcessTimelineForFrameStats(Timeline, BucketMapper, UpdateTotalMinMaxTimerStats, Frames[FrameIndex].StartTime, Frames[FrameIndex].EndTime, FrameResult);
					}

					typename TMap<BucketKeyType, FAggregatedTimingStats>::TIterator FrameResultIterator(FrameResult);
					typename TMap<BucketKeyType, FAggregatedTimingStats>::TIterator TaskResultIterator(*TaskResult);
					typename TMap<BucketKeyType, FInternalFrameAggregationEntry>::TIterator GlobalResultIterator(GlobalResult);
					while (FrameResultIterator)
					{
						FAggregatedTimingStats& FrameStats = FrameResultIterator->Value;
						FAggregatedTimingStats& TaskResultStats = TaskResultIterator->Value;

						// TODO: Add Instance Count per frame column
						// ResultStats.InstanceCount = (ResultStats.InstanceCount * Index + FrameStats.InstanceCount) / (Index + 1);

						TaskResultStats.InstanceCount += FrameStats.InstanceCount;

						TaskResultStats.TotalInclusiveTime += FrameStats.TotalInclusiveTime;
						TaskResultStats.MinInclusiveTime = FMath::Min(TaskResultStats.MinInclusiveTime, FrameStats.TotalInclusiveTime);
						TaskResultStats.MaxInclusiveTime = FMath::Max(TaskResultStats.MaxInclusiveTime, FrameStats.TotalInclusiveTime);

						TaskResultStats.TotalExclusiveTime += FrameStats.TotalExclusiveTime;
						TaskResultStats.MinExclusiveTime = FMath::Min(TaskResultStats.MinExclusiveTime, FrameStats.TotalExclusiveTime);
						TaskResultStats.MaxExclusiveTime = FMath::Max(TaskResultStats.MaxExclusiveTime, FrameStats.TotalExclusiveTime);

						if (bComputeMedian)
						{
							GlobalResultIterator->Value.FrameInclusiveTimes[FrameIndex] = FrameStats.TotalInclusiveTime;
							GlobalResultIterator->Value.FrameExclusiveTimes[FrameIndex] = FrameStats.TotalExclusiveTime;
						}

						// Reset the per frame stats.
						FrameStats = FAggregatedTimingStats();

						++FrameResultIterator;
						++TaskResultIterator;
						++GlobalResultIterator;
					}

				}

				FrameResultsQueue.Enqueue(MoveTemp(TaskResult));
			}));

			StartIndex = EndIndex;
			EndIndex = StartIndex + FramesPerTask + (ExtraFrameTasks > 0 ? 1 : 0);
			--ExtraFrameTasks;
		}

		check(StartIndex == Frames.Num());

		int ProcessedResults = 0;
		while (ProcessedResults < NumTasks)
		{
			if (CancellationToken.IsValid() && CancellationToken->ShouldCancel())
			{
				UE::Tasks::Wait(AsyncTasks);
				return;
			}

			if (FrameResultsQueue.IsEmpty())
			{
				FPlatformProcess::SleepNoStats(0.1f);
				continue;
			}

			TSharedPtr<TMap<BucketKeyType, FAggregatedTimingStats>> TaskResult;
			ensure(FrameResultsQueue.Dequeue(TaskResult));
			++ProcessedResults;

			typename TMap<BucketKeyType, FAggregatedTimingStats>::TIterator TaskResultIterator(*TaskResult);
			typename TMap<BucketKeyType, FInternalFrameAggregationEntry>::TIterator ResultIterator(GlobalResult);
			while (ResultIterator)
			{
				FAggregatedTimingStats& TaskStats = TaskResultIterator->Value;
				FAggregatedTimingStats& ResultStats = ResultIterator->Value.Inner.Stats;

				// TODO: Add Instance Count per frame column
				// ResultStats.InstanceCount = (ResultStats.InstanceCount * Index + FrameStats.InstanceCount) / (Index + 1);

				ResultStats.InstanceCount += TaskStats.InstanceCount;

				ResultStats.TotalInclusiveTime += TaskStats.TotalInclusiveTime;
				ResultStats.MinInclusiveTime = FMath::Min(ResultStats.MinInclusiveTime, TaskStats.MinInclusiveTime);
				ResultStats.MaxInclusiveTime = FMath::Max(ResultStats.MaxInclusiveTime, TaskStats.MaxInclusiveTime);

				ResultStats.TotalExclusiveTime += TaskStats.TotalExclusiveTime;
				ResultStats.MinExclusiveTime = FMath::Min(ResultStats.MinExclusiveTime, TaskStats.MinExclusiveTime);
				ResultStats.MaxExclusiveTime = FMath::Max(ResultStats.MaxExclusiveTime, TaskStats.MaxExclusiveTime);

				++TaskResultIterator;
				++ResultIterator;
			}
		}

		if (bComputeMedian)
		{
			// Compute the median inclusive and exclusive time.
			for (auto& KV : GlobalResult)
			{
				if (KV.Value.Inner.Stats.InstanceCount == 0)
				{
					// Some timers might have 0 instances because the gathering phase also searched between frames.
					continue;
				}
				PreComputeHistogram(KV.Value.Inner);

				for (int Index = 0; Index < FramesNum; ++Index)
				{
					UpdateHistogramForTimerStats(KV.Value.Inner, KV.Value.FrameInclusiveTimes[Index], KV.Value.FrameExclusiveTimes[Index]);
				}

				ComputeFrameStatsMedian(KV.Value.Inner, FramesNum);
			}
		}

		for (auto& KV : GlobalResult)
		{
			KV.Value.Inner.Stats.AverageInclusiveTime = KV.Value.Inner.Stats.TotalInclusiveTime / FramesNum;
			KV.Value.Inner.Stats.AverageExclusiveTime = KV.Value.Inner.Stats.TotalExclusiveTime / FramesNum;

			Result.Add(KV.Key, KV.Value.Inner.Stats);
		}
	}

private:
	enum
	{
		HistogramLen = 100
	};

	struct FInternalAggregationEntry
	{
		FInternalAggregationEntry()
		{
			FMemory::Memzero(InclHistogram, sizeof(int32) * HistogramLen);
			InclDT = 1.0;

			FMemory::Memzero(ExclHistogram, sizeof(int32) * HistogramLen);
			ExclDT = 1.0;
		}

		// Histogram for computing median inclusive time.
		int32 InclHistogram[HistogramLen];
		double InclDT; // bucket size

		// Histogram for computing median exclusive time.
		int32 ExclHistogram[HistogramLen];
		double ExclDT; // bucket size

		FAggregatedTimingStats Stats;
	};

	struct FInternalFrameAggregationEntry
	{
		FInternalAggregationEntry Inner;

		TArray<double> FrameInclusiveTimes;
		TArray<double> FrameExclusiveTimes;
	};

	template<typename TimelineType, typename BucketMappingFunc, typename BucketKeyType, typename CallbackType>
	static void ProcessTimeline(const TimelineType* Timeline, BucketMappingFunc BucketMapper, CallbackType Callback, double IntervalStart, double IntervalEnd, TMap<BucketKeyType, FInternalAggregationEntry>& InternalResult)
	{
		struct FStackEntry
		{
			double StartTime;
			double ExclusiveTime;
			BucketKeyType BucketKey;
		};

		TArray<FStackEntry> Stack;
		Stack.Reserve(1024);
		double LastTime = 0.0;
		Timeline->EnumerateEvents(IntervalStart, IntervalEnd, [BucketMapper, Callback, IntervalStart, IntervalEnd, &Stack, &LastTime, &InternalResult](bool IsEnter, double Time, const typename TimelineType::EventType& Event)
		{
			Time = FMath::Clamp(Time, IntervalStart, IntervalEnd);
			BucketKeyType BucketKey = BucketMapper(Event);
			if (Stack.Num())
			{
				FStackEntry& StackEntry = Stack.Top();
				StackEntry.ExclusiveTime += Time - LastTime;
			}
			LastTime = Time;
			if (IsEnter)
			{
				FStackEntry& StackEntry = Stack.AddDefaulted_GetRef();
				StackEntry.StartTime = Time;
				StackEntry.ExclusiveTime = 0.0;
				StackEntry.BucketKey = BucketKey;
				if (!InternalResult.Contains(BucketKey))
				{
					InternalResult.Add(BucketKey);
				}
			}
			else
			{
				FStackEntry& StackEntry = Stack.Last();
				double EventInclusiveTime = Time - StackEntry.StartTime;
				check(EventInclusiveTime >= 0.0);
				double EventExclusiveTime = StackEntry.ExclusiveTime;
				check(EventExclusiveTime >= 0.0 && EventExclusiveTime <= EventInclusiveTime);
				Stack.Pop(EAllowShrinking::No);
				double EventNonRecursiveInclusiveTime = EventInclusiveTime;
				for (const FStackEntry& AncestorStackEntry : Stack)
				{
					if (AncestorStackEntry.BucketKey == BucketKey)
					{
						EventNonRecursiveInclusiveTime = 0.0;
					}
				}
				Callback(InternalResult[BucketKey], EventNonRecursiveInclusiveTime, EventExclusiveTime);
			}
			return EEventEnumerate::Continue;
		});
	}

	template<typename TimelineType, typename BucketMappingFunc, typename BucketKeyType, typename CallbackType>
	static void ProcessTimelineForFrameStats(const TimelineType* Timeline, BucketMappingFunc BucketMapper, CallbackType Callback, double IntervalStart, double IntervalEnd, TMap<BucketKeyType, FAggregatedTimingStats>& InternalResult)
	{
		struct FStackEntry
		{
			double StartTime;
			double ExclusiveTime;
			BucketKeyType BucketKey;
		};

		TArray<FStackEntry> Stack;
		Stack.Reserve(1024);
		double LastTime = 0.0;
		Timeline->EnumerateEvents(IntervalStart, IntervalEnd, [BucketMapper, Callback, IntervalStart, IntervalEnd, &Stack, &LastTime, &InternalResult](bool IsEnter, double Time, const typename TimelineType::EventType& Event)
		{
			Time = FMath::Clamp(Time, IntervalStart, IntervalEnd);
			BucketKeyType BucketKey = BucketMapper(Event);
			if (Stack.Num())
			{
				FStackEntry& StackEntry = Stack.Top();
				StackEntry.ExclusiveTime += Time - LastTime;
			}
			LastTime = Time;
			if (IsEnter)
			{
				FStackEntry& StackEntry = Stack.AddDefaulted_GetRef();
				StackEntry.StartTime = Time;
				StackEntry.ExclusiveTime = 0.0;
				StackEntry.BucketKey = BucketKey;
			}
			else
			{
				FStackEntry& StackEntry = Stack.Last();
				double EventInclusiveTime = Time - StackEntry.StartTime;
				check(EventInclusiveTime >= 0.0);
				double EventExclusiveTime = StackEntry.ExclusiveTime;
				check(EventExclusiveTime >= 0.0 && EventExclusiveTime <= EventInclusiveTime);
				Stack.Pop(EAllowShrinking::No);
				double EventNonRecursiveInclusiveTime = EventInclusiveTime;
				for (const FStackEntry& AncestorStackEntry : Stack)
				{
					if (AncestorStackEntry.BucketKey == BucketKey)
					{
						EventNonRecursiveInclusiveTime = 0.0;
					}
				}
				Callback(InternalResult[BucketKey], EventNonRecursiveInclusiveTime, EventExclusiveTime);
			}
			return EEventEnumerate::Continue;
		});
	}

	static void UpdateTotalMinMaxTimerAggregationStats(FInternalAggregationEntry& AggregationEntry, double InclTime, double ExclTime)
	{
		UpdateTotalMinMaxTimerStats(AggregationEntry.Stats, InclTime, ExclTime);
	}

	static void UpdateTotalMinMaxTimerStats(FAggregatedTimingStats& Stats, double InclTime, double ExclTime)
	{
		Stats.TotalInclusiveTime += InclTime;
		if (InclTime < Stats.MinInclusiveTime)
		{
			Stats.MinInclusiveTime = InclTime;
		}
		if (InclTime > Stats.MaxInclusiveTime)
		{
			Stats.MaxInclusiveTime = InclTime;
		}

		Stats.TotalExclusiveTime += ExclTime;
		if (ExclTime < Stats.MinExclusiveTime)
		{
			Stats.MinExclusiveTime = ExclTime;
		}
		if (ExclTime > Stats.MaxExclusiveTime)
		{
			Stats.MaxExclusiveTime = ExclTime;
		}

		Stats.InstanceCount++;
	}

	static void PreComputeHistogram(FInternalAggregationEntry& AggregationEntry)
	{
		const FAggregatedTimingStats& Stats = AggregationEntry.Stats;

		// Each bucket (Histogram[i]) will be centered on a value.
		// I.e. First bucket (bucket 0) is centered on Min value: [Min-DT/2, Min+DT/2)
		// and last bucket (bucket N-1) is centered on Max value: [Max-DT/2, Max+DT/2).

		constexpr double InvHistogramLen = 1.0 / static_cast<double>(HistogramLen - 1);

		if (Stats.MaxInclusiveTime == Stats.MinInclusiveTime)
		{
			AggregationEntry.InclDT = 1.0; // single large bucket
		}
		else
		{
			AggregationEntry.InclDT = (Stats.MaxInclusiveTime - Stats.MinInclusiveTime) * InvHistogramLen;
		}

		if (Stats.MaxExclusiveTime == Stats.MinExclusiveTime)
		{
			AggregationEntry.ExclDT = 1.0; // single large bucket
		}
		else
		{
			AggregationEntry.ExclDT = (Stats.MaxExclusiveTime - Stats.MinExclusiveTime) * InvHistogramLen;
		}
	}

	static void UpdateHistogramForTimerStats(FInternalAggregationEntry& AggregationEntry, double InclTime, double ExclTime)
	{
		const FAggregatedTimingStats& Stats = AggregationEntry.Stats;

		int32 InclIndex = static_cast<int32>((InclTime - Stats.MinInclusiveTime + AggregationEntry.InclDT / 2) / AggregationEntry.InclDT);
		ensure(InclIndex >= 0);
		if (InclIndex < 0)
		{
			InclIndex = 0;
		}
		ensure(InclIndex < HistogramLen);
		if (InclIndex >= HistogramLen)
		{
			InclIndex = HistogramLen - 1;
		}
		AggregationEntry.InclHistogram[InclIndex]++;

		int32 ExclIndex = static_cast<int32>((ExclTime - Stats.MinExclusiveTime + AggregationEntry.ExclDT / 2) / AggregationEntry.ExclDT);
		ensure(ExclIndex >= 0);
		if (ExclIndex < 0)
		{
			ExclIndex = 0;
		}
		ensure(ExclIndex < HistogramLen);
		if (ExclIndex >= HistogramLen)
		{
			ExclIndex = HistogramLen - 1;
		}
		AggregationEntry.ExclHistogram[ExclIndex]++;
	}

	static void PostProcessTimerStats(FInternalAggregationEntry& AggregationEntry, bool ComputeMedian)
	{
		FAggregatedTimingStats& Stats = AggregationEntry.Stats;

		// Compute average inclusive/exclusive times.
		ensure(Stats.InstanceCount > 0);
		double InvCount = 1.0f / static_cast<double>(Stats.InstanceCount);
		Stats.AverageInclusiveTime = Stats.TotalInclusiveTime * InvCount;
		Stats.AverageExclusiveTime = Stats.TotalExclusiveTime * InvCount;

		if (ComputeMedian)
		{
			const int32 HalfCount = static_cast<int32>(Stats.InstanceCount / 2);

			// Compute median inclusive time.
			int32 InclCount = 0;
			for (int32 HistogramIndex = 0; HistogramIndex < HistogramLen; HistogramIndex++)
			{
				InclCount += AggregationEntry.InclHistogram[HistogramIndex];
				if (InclCount > HalfCount)
				{
					Stats.MedianInclusiveTime = Stats.MinInclusiveTime + HistogramIndex * AggregationEntry.InclDT;
					if (HistogramIndex > 0 &&
						Stats.InstanceCount % 2 == 0 &&
						InclCount - AggregationEntry.InclHistogram[HistogramIndex] == HalfCount)
					{
						const double PrevMedian = Stats.MinInclusiveTime + (HistogramIndex - 1) * AggregationEntry.InclDT;
						Stats.MedianInclusiveTime = (Stats.MedianInclusiveTime + PrevMedian) / 2;
					}
					break;
				}
			}

			// Compute median exclusive time.
			int32 ExclCount = 0;
			for (int32 HistogramIndex = 0; HistogramIndex < HistogramLen; HistogramIndex++)
			{
				ExclCount += AggregationEntry.ExclHistogram[HistogramIndex];
				if (ExclCount > HalfCount)
				{
					Stats.MedianExclusiveTime = Stats.MinExclusiveTime + HistogramIndex * AggregationEntry.ExclDT;
					if (HistogramIndex > 0 &&
						Stats.InstanceCount % 2 == 0 &&
						ExclCount - AggregationEntry.ExclHistogram[HistogramIndex] == HalfCount)
					{
						const double PrevMedian = Stats.MinExclusiveTime + (HistogramIndex - 1) * AggregationEntry.ExclDT;
						Stats.MedianExclusiveTime = (Stats.MedianExclusiveTime + PrevMedian) / 2;
					}
					break;
				}
			}
		}
	}

	static void ComputeFrameStatsMedian(FInternalAggregationEntry& AggregationEntry, uint32 NumFrames)
	{
		FAggregatedTimingStats& Stats = AggregationEntry.Stats;

		const int32 HalfCount = static_cast<int32>(NumFrames / 2);

		// Compute median inclusive time.
		int32 InclCount = 0;
		for (int32 HistogramIndex = 0; HistogramIndex < HistogramLen; HistogramIndex++)
		{
			InclCount += AggregationEntry.InclHistogram[HistogramIndex];
			if (InclCount > HalfCount)
			{
				Stats.MedianInclusiveTime = Stats.MinInclusiveTime + HistogramIndex * AggregationEntry.InclDT;
				if (HistogramIndex > 0 &&
					NumFrames % 2 == 0 &&
					InclCount - AggregationEntry.InclHistogram[HistogramIndex] == HalfCount)
				{
					const double PrevMedian = Stats.MinInclusiveTime + (HistogramIndex - 1) * AggregationEntry.InclDT;
					Stats.MedianInclusiveTime = (Stats.MedianInclusiveTime + PrevMedian) / 2;
				}
				break;
			}
		}

		// Compute median exclusive time.
		int32 ExclCount = 0;
		for (int32 HistogramIndex = 0; HistogramIndex < HistogramLen; HistogramIndex++)
		{
			ExclCount += AggregationEntry.ExclHistogram[HistogramIndex];
			if (ExclCount > HalfCount)
			{
				Stats.MedianExclusiveTime = Stats.MinExclusiveTime + HistogramIndex * AggregationEntry.ExclDT;
				if (HistogramIndex > 0 &&
					NumFrames % 2 == 0 &&
					ExclCount - AggregationEntry.ExclHistogram[HistogramIndex] == HalfCount)
				{
					const double PrevMedian = Stats.MinExclusiveTime + (HistogramIndex - 1) * AggregationEntry.ExclDT;
					Stats.MedianExclusiveTime = (Stats.MedianExclusiveTime + PrevMedian) / 2;
				}
				break;
			}
		}
	}

	template<typename TimelineType, typename BucketMappingFunc, typename BucketKeyType>
	static void GatherKeysFromTimeline(const TimelineType* Timeline, BucketMappingFunc BucketMapper, double IntervalStart, double IntervalEnd, TMap<BucketKeyType, FAggregatedTimingStats>& InternalResult)
	{
		Timeline->EnumerateEvents(IntervalStart, IntervalEnd, [BucketMapper, IntervalStart, IntervalEnd, &InternalResult](bool IsEnter, double Time, const typename TimelineType::EventType& Event)
			{
				BucketKeyType BucketKey = BucketMapper(Event);

				if (IsEnter)
				{
					if (!InternalResult.Contains(BucketKey))
					{
						InternalResult.Add(BucketKey);
					}
				}

				return EEventEnumerate::Continue;
			});
	}
};

} // namespace TraceServices
