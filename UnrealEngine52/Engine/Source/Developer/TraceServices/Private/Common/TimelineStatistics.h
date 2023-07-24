// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "CoreTypes.h"
#include <cfloat>
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

class FTimelineStatistics
{
public:
	template<typename TimelineType, typename BucketMappingFunc, typename BucketKeyType>
	static void CreateAggregation(const TArray<const TimelineType*>& Timelines, BucketMappingFunc BucketMapper, double IntervalStart, double IntervalEnd, TMap<BucketKeyType, FAggregatedTimingStats>& Result)
	{
		TMap<BucketKeyType, FInternalAggregationEntry> InternalResult;
		// Compute instance count and total/min/max inclusive/exclusive times for each timer.
		for (const TimelineType* Timeline : Timelines)
		{
			ProcessTimeline(Timeline, BucketMapper, UpdateTotalMinMaxTimerStats, IntervalStart, IntervalEnd, InternalResult);
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
				ProcessTimeline(Timeline, BucketMapper, UpdateHistogramForTimerStats, IntervalStart, IntervalEnd, InternalResult);
			}
		}

		// Compute average and median inclusive/exclusive times.
		for (auto& KV : InternalResult)
		{
			PostProcessTimerStats(KV.Value, ComputeMedian);
			Result.Add(KV.Key, KV.Value.Stats);
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
				Stack.Pop(false);
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

	static void UpdateTotalMinMaxTimerStats(FInternalAggregationEntry& AggregationEntry, double InclTime, double ExclTime)
	{
		FAggregatedTimingStats& Stats = AggregationEntry.Stats;
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
				ExclCount += AggregationEntry.InclHistogram[HistogramIndex];
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
};

} // namespace TraceServices
