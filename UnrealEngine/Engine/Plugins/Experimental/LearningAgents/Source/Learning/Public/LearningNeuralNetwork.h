// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

namespace UE::Learning
{
	/**
	* Activation Function for use in a Neural Network
	*/
	enum class EActivationFunction : uint8
	{
		// ReLU Activation - Fast to train and evaluate but occasionally causes gradient collapse and untrainable networks.
		ReLU = 0,

		// ELU Activation - Generally performs better than ReLU and is not prone to gradient collapse but slower to evaluate.
		ELU = 1,

		// TanH Activation - Smooth activation function that is slower to train and evaluate but sometimes more stable for certain tasks.
		TanH = 2,
	};

	LEARNING_API const TCHAR* GetActivationFunctionString(const EActivationFunction ActivationFunction);

	/**
	* Simple feed-forward network data.
	*/
	struct LEARNING_API FNeuralNetwork
	{
		void DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes);
		void SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const;

		static int32 GetSerializationByteNum(
			const int32 InputNum,
			const int32 OutputNum,
			const int32 HiddenNum,
			const int32 LayerNum);

		void Resize(
			const int32 InputNum,
			const int32 OutputNum,
			const int32 HiddenNum,
			const int32 LayerNum);

		int32 GetInputNum() const;
		int32 GetOutputNum() const;
		int32 GetLayerNum() const;
		int32 GetHiddenNum() const;

		EActivationFunction ActivationFunction = EActivationFunction::ELU;
		TArray<TLearningArray<2, float>, TInlineAllocator<16>> Weights;
		TArray<TLearningArray<1, float>, TInlineAllocator<16>> Biases;
	};
}
