// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPoint.h"
#include "Helpers/PCGAsync.h"

#include "Algo/Transform.h"

namespace FPCGSpatialDataProcessing
{
	constexpr int32 DefaultSamplePointsChunkSize = 256;

	template<int ChunkSize, typename ProcessRangeFunc>
	void SampleBasedRangeProcessing(FPCGAsyncState* AsyncState, ProcessRangeFunc&& InProcessRange, const TArray<FPCGPoint>& SourcePoints, TArray<FPCGPoint>& OutPoints)
	{
		const int NumIterations = SourcePoints.Num();

		auto Initialize = [&OutPoints, NumIterations]()
		{
			OutPoints.SetNumUninitialized(NumIterations);
		};

		auto ProcessRange = [RangeFunc = MoveTemp(InProcessRange), &SourcePoints, &OutPoints](int32 StartReadIndex, int32 StartWriteIndex, int32 Count) -> int32
		{
			ensure(Count >= 0 && Count <= ChunkSize);

			TArrayView<const FPCGPoint> IterationPoints(SourcePoints.GetData() + StartReadIndex, Count);
			TArray<TPair<FTransform, FBox>, TInlineAllocator<ChunkSize>> Samples;
			TArray<FPCGPoint, TInlineAllocator<ChunkSize>> RangeOutputPoints;

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpatialDataProcessing::SamplePoints::PrepareSamples)
				Algo::Transform(IterationPoints, Samples, [](const FPCGPoint& Point) { return TPair<FTransform, FBox>(Point.Transform, Point.GetLocalBounds()); });
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpatialDataProcessing::SamplePoints::RangeFunc)
				RangeFunc(Samples, IterationPoints, RangeOutputPoints);
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpatialDataProcessing::SamplePoints::RangeCopyResults)
				// Copy back the points
				FMemory::Memcpy((void*)(OutPoints.GetData() + StartWriteIndex), (void*)(RangeOutputPoints.GetData()), sizeof(FPCGPoint) * RangeOutputPoints.Num());
			}

			return RangeOutputPoints.Num();
		};

		auto MoveDataRange = [&OutPoints](int32 ReadIndex, int32 WriteIndex, int32 Count)
		{
			// Implementation note: if the array is to be moved to a range partially overlapping itself, it's important not to use Memcpy here
			FMemory::Memmove((void*)(OutPoints.GetData() + WriteIndex), (void*)(OutPoints.GetData() + ReadIndex), sizeof(FPCGPoint) * Count);
		};

		auto Finished = [&OutPoints](int32 Count)
		{
			// Shrinking can have a big impact on the performance, but without it, we can also hold a big chunk of wasted memory.
			// Might revisit later if the performance impact is too big.
			OutPoints.SetNum(Count);
		};

		ensure(FPCGAsync::AsyncProcessingRangeEx(AsyncState, NumIterations, Initialize, ProcessRange, MoveDataRange, Finished, /*bEnableTimeSlicing=*/false, ChunkSize));
	}
}