// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralTensor.h"
#include "Algo/Accumulate.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeuralNetworkInference, Display, All);
DECLARE_STATS_GROUP(TEXT("MachineLearning"), STATGROUP_MachineLearning, STATCAT_Advanced);

class FNeuralNetworkInferenceUtils
{
public:
	/**
	 * SizeSanityChecks() makes sure the tensor has the expected size (either InNum or between InMinNum and InMaxNum) and that all elements are not nullptr.
	 * @param InMinNum/InMaxNum: Range of elements that InTensorArray.Num() must satisfy.
	 * @param InMinDimensions/InMaxDimensions: If not -1, range of dimensions that each element in InTensorArray.Num() must satisfy.
	 * @param InMinDimensionsFirst/InMaxDimensionsLast: If not -1, element range for which InMinDimensions/InMaxDimensions is applied to.
	 */
	FORCEINLINE static bool SizeSanityChecks(const TArray<FNeuralTensor*>& InTensorArray, const int32 InNum);
	static bool SizeSanityChecks(const TArray<FNeuralTensor*>& InTensorArray, const int32 InMinNum, const int32 InMaxNum,
		const int32 InMinDimensions = -1, const int32 InMaxDimensions = -1, const int32 InDimensionRangeFirst = -1, const int32 InDimensionRangeLast = -1);

	template <typename T>
	struct MultiplyOperation
	{
		constexpr T operator()(const T& InLeft, const T& InRight) const { return InLeft * InRight; }
	};

	template <typename ValueType, typename ContainerType>
	static ValueType Product(const ContainerType& InContainer);

	/**
	 * It blocks the current thread until the RHI thread has finished all instructions before this point.
	 * @return The time it waited.
	 */
	static void WaitUntilRHIFinished();
};



// Inline implementations
bool FNeuralNetworkInferenceUtils::SizeSanityChecks(const TArray<FNeuralTensor*>& InTensorArray, const int32 InNum)
{
	return SizeSanityChecks(InTensorArray, InNum, InNum);
}

template <typename ValueType, typename ContainerType>
ValueType FNeuralNetworkInferenceUtils::Product(const ContainerType& InContainer)
{
	return Algo::Accumulate(InContainer, ValueType(1), MultiplyOperation<ValueType>());
}
