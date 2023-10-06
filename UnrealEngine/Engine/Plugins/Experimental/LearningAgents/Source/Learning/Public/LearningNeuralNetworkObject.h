// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArrayMap.h"
#include "LearningFunctionObject.h"

#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	struct FNeuralNetwork;

	/**
	* Settings object for a neural network based policy
	*/
	struct LEARNING_API FNeuralNetworkPolicyFunctionSettings
	{
		// Minimum amount of action noise to allow
		float ActionNoiseMin = 0.0f;

		// Maximum amount of action noise to allow
		float ActionNoiseMax = 0.0f;

		// Overall scale of the action noise
		float ActionNoiseScale = 1.0f;

		// If to allow for multi-threaded evaluation
		bool bParallelEvaluation = true;

		// Minimum batch size to use for multi-threaded evaluation
		uint16 MinParallelBatchSize = 16;
	};

	/**
	* Neural-network based policy object. Stores various settings and intermediate
	* storage required to evaluate the given network for the provided number of instances.
	*/
	struct LEARNING_API FNeuralNetworkPolicyFunction : public FFunctionObject
	{
		FNeuralNetworkPolicyFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const TSharedRef<FNeuralNetwork>& InNeuralNetwork,
			const uint32 InSeed,
			const FNeuralNetworkPolicyFunctionSettings& InSettings = FNeuralNetworkPolicyFunctionSettings());

		virtual void Evaluate(const FIndexSet Instances) override final;

		TSharedRef<FNeuralNetwork> NeuralNetwork;
		FNeuralNetworkPolicyFunctionSettings Settings;

		TArray<TLearningArray<2, float, TInlineAllocator<128>>, TInlineAllocator<16>> Activations;

		TArrayMapHandle<1, uint32> SeedHandle;
		TArrayMapHandle<2, float> InputHandle;
		TArrayMapHandle<2, float> OutputHandle;
		TArrayMapHandle<2, float> OutputMeanHandle;
		TArrayMapHandle<2, float> OutputStdHandle;
		TArrayMapHandle<1, float> ActionNoiseScaleHandle;
	};

	/**
	* Settings object for a neural network based critic
	*/
	struct LEARNING_API FNeuralNetworkCriticFunctionSettings
	{
		// If to allow for multi-threaded evaluation
		bool bParallelEvaluation = true;

		// Minimum batch size to use for multi-threaded evaluation
		uint16 MinParallelBatchSize = 16;
	};

	/**
	* Neural-network based critic object. Stores various settings and intermediate
	* storage required to evaluate the given network for the provided number of instances.
	*/
	struct LEARNING_API FNeuralNetworkCriticFunction : public FFunctionObject
	{
		FNeuralNetworkCriticFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const TSharedRef<FNeuralNetwork>& InNeuralNetwork,
			const FNeuralNetworkCriticFunctionSettings& InSettings = FNeuralNetworkCriticFunctionSettings());

		virtual void Evaluate(const FIndexSet Instances) override final;

		TSharedRef<FNeuralNetwork> NeuralNetwork;
		FNeuralNetworkCriticFunctionSettings Settings;

		TArray<TLearningArray<2, float, TInlineAllocator<128>>, TInlineAllocator<16>> Activations;

		TArrayMapHandle<2, float> InputHandle;
		TArrayMapHandle<1, float> OutputHandle;
	};
}
