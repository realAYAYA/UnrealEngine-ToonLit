// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArrayMap.h"

#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	namespace RecorderObjectPrivate
	{
		template<uint8 DimNum, typename ElementType>
		void Copy(
			TLearningArrayView<1, int32> InOutStepNums,
			const FArrayMap& SrcArrayMap,
			const FArrayMap& LogArrayMap,
			const FArrayMapHandle SrcHandle,
			const FArrayMapHandle LogHandle,
			const int32 MaxStepNum,
			const FIndexSet Instances)
		{
			const TLearningArrayView<DimNum, const ElementType> SrcView = SrcArrayMap.ConstView<DimNum, ElementType>(SrcHandle);
			TLearningArrayView<DimNum + 1, ElementType> LogView = LogArrayMap.View<DimNum + 1, ElementType>(LogHandle);

			for (const int32 InstanceIdx : Instances)
			{
				const int32 StepNum = InOutStepNums[InstanceIdx];

				if (StepNum < MaxStepNum)
				{
					if constexpr (DimNum == 1)
					{
						LogView[StepNum][InstanceIdx] = SrcView[InstanceIdx];
					}
					else
					{
						Array::Copy(LogView[StepNum][InstanceIdx], SrcView[InstanceIdx]);
					}

					InOutStepNums[InstanceIdx]++;
				}
			}
		}
	}

	/**
	* The `RecorderObject` can be used to record the values of certain arrays in an `FArrayMap`. 
	* This can be useful for debugging since we can keep track of certain arrays over time and 
	* compute statistics or visualize them.
	* 
	* This object assumes the first dimension on all arrays corresponds to `Instances`, as it works by
	* only record updates and resets for certain `Instances` given by an `FIndexSet`. This makes it 
	* easier to use in conjunction with the other objects that use `FArrayMap` such as `FRewardObject`.
	*/
	struct LEARNING_API FRecorderObject
	{
		FRecorderObject(
			const int32 InMaxInstanceNum,
			const int32 InMaxStepNum,
			const TSharedRef<FArrayMap>& InSrcArrayMap,
			const TSharedRef<FArrayMap>& InLogArrayMap);

		~FRecorderObject();

		/**
		* Record an array from the provided `SrcArrayMap` in the `LogArrayMap`
		*/
		template<uint8 DimNum, typename ElementType>
		void Add(const FArrayMapKey Key)
		{
			const FArrayMapHandle SrcHandle = SrcArrayMap->Lookup(Key);

			const TLearningArrayView<DimNum, const ElementType> SrcView = SrcArrayMap->ConstView<DimNum, ElementType>(SrcHandle);
			UE_LEARNING_ARRAY_SHAPE_CHECK(SrcView.template Num<0>() == MaxInstanceNum);

			TMultiArrayShape<DimNum + 1> LogShape;
			LogShape[0] = MaxStepNum;
			for (int32 DimIdx = 0; DimIdx < DimNum; DimIdx++)
			{
				LogShape[1 + DimIdx] = SrcView.Num(DimIdx);
			}

			const FArrayMapHandle LogHandle = LogArrayMap->Add<DimNum + 1, ElementType>(Key, LogShape);

			SrcHandles.Emplace(SrcHandle);
			LogHandles.Emplace(LogHandle);
			CopyFunctions.Emplace(RecorderObjectPrivate::Copy<DimNum, ElementType>);
		}

		TLearningArrayView<1, const int32> GetStepNums() const { return StepNums; }

		/**
		* Reset all recorded arrays
		*/
		void Reset(const FIndexSet Instances);

		/**
		* Update all recorded arrays (record their current value)
		*/
		void Update(const FIndexSet Instances);

	private:

		int32 MaxInstanceNum = 0;
		int32 MaxStepNum = 0;
		TSharedRef<FArrayMap> SrcArrayMap;
		TSharedRef<FArrayMap> LogArrayMap;

		TLearningArray<1, int32> StepNums;
		TArray<FArrayMapHandle, TInlineAllocator<32>> SrcHandles;
		TArray<FArrayMapHandle, TInlineAllocator<32>> LogHandles;

		TArray<TFunctionRef<void(
			TLearningArrayView<1, int32> InOutStepNums,
			const FArrayMap& SrcArrayMap,
			const FArrayMap& LogArrayMap,
			const FArrayMapHandle SrcHandle,
			const FArrayMapHandle LogHandle,
			const int32 MaxStepNum,
			const FIndexSet Instances)>, TInlineAllocator<32>> CopyFunctions;
	};
}
