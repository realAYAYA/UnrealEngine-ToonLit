// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralNetwork.h"

#include <atomic>
#include "ModelProto.h"
#include "NeuralOperator.h"
#include "UEOnly/NeuralTensorManager.h"

/* FImplBackEndUEOnly
 *****************************************************************************/

struct UNeuralNetwork::FImplBackEndUEOnly
{
	/**
	 * Whether an inference pass (i.e., Run) is happening.
	 * This variable is thread safe as long as only "Run" modifies it. Other functions can safely read it at any time.
	 * If other functions outside of Run() have to modify it, consider using a mutex with a bool rather than just a std::atomic<bool>.
	 */
	std::atomic<bool> bIsBackgroundThreadRunning;

	/**
	 * It should always be false when loaded from uasset (FNeuralTensors are not auto-loaded to GPU).
	 */
	bool bAreTensorsInGpu;

	FModelProto ModelProto;

	/**
	 * It contains a few TArray and TMaps for all FNeuralTensors (Input, Output, Intermediate(Not)Initialized, Weight).
	 */
	FNeuralTensorManager TensorManager;

	/**
	 * Only for the vanilla back end.
	 * Set of operators that the network need to run on the Forward pass and that might need to run on the PostForward pass.
	 */
	TArray<TSharedPtr<FNeuralOperator>> Operators;

	~FImplBackEndUEOnly();

	static bool Load(TSharedPtr<FImplBackEndUEOnly>& InOutImplBackEndUEOnly, const TArray<uint8>& InModelReadFromFileInBytes);
	static bool Load(TSharedPtr<FImplBackEndUEOnly>& InOutImplBackEndUEOnly, FNeuralTensorManager& InTensorManager, const TArray<TSharedPtr<FNeuralOperator>>& InOperators);

	void Run(FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate, const ENeuralSynchronousMode InSynchronousMode, const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType);

private:
	/**
	 *  Used by both Load() functions to reset InOutImplBackEndUEOnly.
	 */
	static void Reset(TSharedPtr<FImplBackEndUEOnly>& InOutImplBackEndUEOnly);
};
