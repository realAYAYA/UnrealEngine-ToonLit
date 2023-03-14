// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralNetwork.h"

class FModelUnitTester
{
public:
	static bool GlobalTest(const FString& InProjectContentDir, const FString& InModelZooRelativeDirectory);

private:
	/**
	 * It runs a full model test on the desired model.
	 */
	static bool ModelLoadAccuracyAndSpeedTests(const FString& InProjectContentDir, const FString& InModelZooRelativeDirectory, const TArray<FString>& InModelNames,
		const TArray<float>& InInputArrayValues, const TArray<TArray<double>>& InCPUGroundTruths, const TArray<TArray<double>>& InGPUGroundTruths,
		const TArray<int32>& InCPURepetitionsForUEAndORT, const TArray<int32>& InGPURepetitionsForUEAndORTBackEnd, const TArray<int32>& InCPURepetitionsForUEOnly,
		const TArray<int32>& InGPURepetitionsForUEOnly);
	/**
	 * Other auxiliary functions for GlobalTest().
	 */
	static FString GetONNXModelFilePath(const FString& InModelZooDirectory, const FString& InModelName);
	static FString GetORTModelFilePath(const FString& InModelZooDirectory, const FString& InModelName);
	static FString GetUAssetModelFilePath(const FString& InModelName, const FString& InModelZooRelativeDirectory);
	static UNeuralNetwork* NetworkUassetLoadTest(const FString& InUAssetPath);
	static UNeuralNetwork* NetworkONNXOrORTLoadTest(const FString& InModelFilePath);
	static bool ModelAccuracyTest(UNeuralNetwork* InOutNetwork, const ENeuralSynchronousMode InSynchronousMode, const UNeuralNetwork::ENeuralBackEnd InBackEnd, const TArray<float>& InInputArrayValues,
		const TArray<double>& InCPUGroundTruths, const TArray<double>& InGPUGroundTruths);
	static void ModelAccuracyTestRun(TArray<TArray<float>>& OutOutputs, UNeuralNetwork* InOutNetwork, const TArray<TArray<float>> InInputArrays,
		const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType);
	static bool ModelSpeedTest(const FString& InUAssetPath, const ENeuralDeviceType InDeviceType, const UNeuralNetwork::ENeuralBackEnd InBackEnd, const int32 InRepetitions);
	static double GetAveragedL1Norm(const TArray<float>& InArray);
	static double GetAveragedL1NormDiff(const TArray<float>& InArray1, const TArray<float>& InArray2);
	/**
	 * Verbose string auxiliary functions.
	 */
	static FString GetDeviceTypeString(const ENeuralDeviceType InDeviceType);
	static FString GetBackEndString(const UNeuralNetwork::ENeuralBackEnd InBackEnd);
};
