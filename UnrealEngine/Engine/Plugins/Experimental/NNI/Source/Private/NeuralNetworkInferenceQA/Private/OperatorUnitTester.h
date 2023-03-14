// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralTensor.h"

class FOperatorUnitTester
{
public:
	static bool GlobalTest(const FString& InProjectContentDir, const FString& InUnitTestRelativeDirectory);

private:
	static void CreateRandomArraysOfFixedSeedAndDefinedSizes(TMap<int32, TArray<float>>& InOutInputTensorData, TMap<int32, TArray<float>>& InOutInputTensorDataSmall,
		TMap<int32, TArray<float>>& InOutInputTensorDataTiny, TMap<int32, TArray<float>>& InOutInputTensorDataPos, const TArray<int32>& InArraySizesToPopulate);

	static TArray<float> CreateRandomArray(struct FRandomStream* InOutRandomStream, const int32 InSize, const float InRangeMin, const float InRangeMax);
};
