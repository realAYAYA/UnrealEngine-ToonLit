// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralNetwork.h"
#include "NeuralTensor.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeuralNetworkInferenceQA, Display, All);

class FNeuralNetworkInferenceQAUtils
{
public:
	/**
	 * It calculates the error between 2 data arrays (it should return 0 if both arrays are the same).
	 * @return False if the estimated error > InZeroThreshold, true otherwise.
	 */
	static bool EstimateTensorL1DiffError(const FNeuralTensor& InTensorA, const FNeuralTensor& InTensorB, const float InZeroThreshold,
		const FString& InDebugName);

	/**
	 * Slow functions (as they will copy every input/output FNeuralNetwork) only meant for debugging purposes.
	 * They really only copy the TArray<uint8> of underlying data, so they are confusing functions because they do not copy any other FNeuralTensor
	 * properties.
	 */
	static TArray<FNeuralTensor> CreateInputArrayCopy(const UNeuralNetwork* const InNeuralNetwork);
	static void SetInputFromArrayCopy(UNeuralNetwork* InOutNeuralNetwork, const TArray<FNeuralTensor>& InTensorDataArray);
	static TArray<FNeuralTensor> CreateOutputArrayCopy(const UNeuralNetwork* const InNeuralNetwork);
};
