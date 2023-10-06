// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningArray.h"

#include "Async/ParallelFor.h"

namespace UE::Learning
{
	bool FIndexSet::TryMakeSlice()
	{
		if (IsSlice())
		{
			return true;
		}

		const int32 IdxNum = Indices.Num<0>();

		if (IdxNum == 0)
		{
			SliceStart = 0;
			SliceNum = 0;
			Indices = TLearningArrayView<1, const int32>();
			return true;
		}

		if (IdxNum == 1)
		{
			SliceStart = Indices[0];
			SliceNum = 1;
			Indices = TLearningArrayView<1, const int32>();
			return true;
		}

		int32 MinIdx = INT32_MAX;
		int32 MaxIdx = INT32_MIN;
		for (int32 Idx = 0; Idx < IdxNum; Idx++)
		{
			MinIdx = FMath::Min(MinIdx, Indices[Idx]);
			MaxIdx = FMath::Max(MaxIdx, Indices[Idx]);
		}

		if (MaxIdx - MinIdx + 1 == IdxNum)
		{
			SliceStart = MinIdx;
			SliceNum = IdxNum;
			Indices = TLearningArrayView<1, const int32>();
			return true;
		}
		
		return false;
	}

	void SlicedParallelFor(const int32 Num, const int32 MinSliceElementNum, const TFunctionRef<void(int32 Start, int32 Length)> Body)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::SlicedParallelFor);

		UE_LEARNING_CHECK(Num >= 0);

		if (Num <= 0)
		{
			return;
		}

		UE_LEARNING_CHECK(MinSliceElementNum >= 1);

		const int32 IdealTaskNum = ParallelForImpl::GetNumberOfThreadTasks(Num, MinSliceElementNum, EParallelForFlags::None);
		const int32 IdealSliceLength = FMath::DivideAndRoundUp(Num, IdealTaskNum);
		const int32 ActualTaskNum = FMath::DivideAndRoundUp(Num, IdealSliceLength);

		if (ActualTaskNum == 1)
		{
			Body(0, Num);
		}
		else
		{
			ParallelFor(ActualTaskNum, [IdealSliceLength, Num, Body](int32 Index)
			{
				const int32 StartIndex = Index * IdealSliceLength;
				const int32 StopIndex = FMath::Min((Index + 1) * IdealSliceLength, Num);
				Body(StartIndex, StopIndex - StartIndex);
			});
		}
	}
}
