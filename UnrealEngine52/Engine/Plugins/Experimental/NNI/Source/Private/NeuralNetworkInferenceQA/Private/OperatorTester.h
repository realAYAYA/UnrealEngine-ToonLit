// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralEnumClasses.h"
#include "NeuralNetwork.h"
#include "NeuralNetworkInferenceQAUtils.h"
#include "NeuralNetworkInferenceQAAsset.h"
#include "NeuralOperator.h"
#include "NeuralTensor.h"

class FOperatorTester
{
public:
	/**
	 * @param InOutOutputTensors It must be empty if the operator is inlined.
	 * @param bInShouldOperatorsProvideSameResult If true, each element of InOutOperators is supposed to give the same exact result (so it will be stored with the same name).
	 * @param bInShouldRunFinalStringVsIOTest If true, the FString InOutAllNetworkStrings will be compared against the GT file. If false, the FString will just be returned
	 * to the caller. The caller can stack and accumulate the FString and compare it against a global GT file (benefit: 1 file per layer).
	 */
	static void TestOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, TArray<TArray<FNeuralTensor>>& InOutTensors, TArray<TArray<FNeuralTensor*>>& InOutInputTensors,
		TArray<TArray<FNeuralTensor*>>& InOutOutputTensors, TArray<TSharedPtr<FNeuralOperator>>& InOutOperators, const FString& InOperatorName,
		const FString& InGroundTruthDirectory, const float InZeroThreshold, const bool bInShouldOperatorsProvideSameResult,
		const bool bInShouldRunGPU = true, const bool bInShouldRunFinalStringVsIOTest = true);

	static void TestBatchNormalizationOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataSmall,
		const TMap<int32, TArray<float>>& InInputTensorDataPos, const float InZeroThreshold, const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	static void TestGemmOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData, const float InZeroThreshold, const FString& InGroundTruthDirectory,
		const int32 InOperatorCounter, const int32 InOperatorTotal);

	static void TestConvOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataSmall,
		const float InZeroThreshold, const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	static void TestConvTransposeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataSmall,
		const float InZeroThreshold, const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	template<typename TNeuralOperatorChild>
	static void TestElementWiseOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TCHAR* InOperatorName, const TArray<TArray<FNeuralTensor>>& InTensors, const float InZeroThreshold,
		const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	template<typename TNeuralOperatorChild>
	static void TestElementWise1AttributeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TCHAR* InOperatorName, const TArray<TArray<FNeuralTensor>>& InTensors, const float InZeroThreshold,
		const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	template<typename TNeuralOperatorChild>
	static void TestMultiDirectionalBroadcastingOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TCHAR* InOperatorName, const TArray<TArray<FNeuralTensor>>& InTensors, const float InZeroThreshold,
		const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	static void TestReshapeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData,
		const TMap<int32, TArray<float>>& InInputTensorDataPos, const float InZeroThreshold, const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

	static void TestSqueezeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData,
		const TMap<int32, TArray<float>>& InInputTensorDataPos, const float InZeroThreshold, const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal);

private:
	static TArray<int64> FloatTensorToIntArray(const TArray<FNeuralTensor>& InTensors, const int32 InTensorIndex);

	/**
	 * For inlined layers, it resets the memory (meant after a UNeuralNetwork::Run()). For non-inlined layers, it checks that the input did not change.
	 */
	static void ResetOrCheckInput(UNeuralNetwork* InOutNetwork, const TArray<FNeuralTensor>& InInputTensorArray, const bool bIsInlinedTensor);

	/**
	 * It tests and makes sure CPU and GPU modes provide the same results. Otherwise, it will check to false.
	 * @param InOutNetwork UNeuralNetwork to test.
	 * @param If not inlined, it will make check whether the input tensors have not changed after running.
	 * @param InZeroThreshold If the average L2 norm between CPU and GPU results is greater than this number, it will check to false.
	 */
	static void NetworkGPUvsCPU(UNeuralNetwork* InOutNetwork, const bool bIsInlinedTensor, const float InZeroThreshold = 0.f);

	/**
	 * If layer inlined --> Check input == output
	 * This is only meant for networks with single input single output, whose layers are all inlined (so that input tensor data should match
	 * output tensor data after a UNeuralNetwork run).
	 */
	static void NetworkInputVsOutput(UNeuralNetwork* InOutNetwork, const int32 InInlinedTensorIndex);
};



/* FOperatorTester inlined and templated functions
 *****************************************************************************/

template<typename TNeuralOperatorChild>
void FOperatorTester::TestElementWiseOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TCHAR* InOperatorName, const TArray<TArray<FNeuralTensor>>& InTensors, const float InZeroThreshold,
	const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d %s"), InOperatorCounter, InOperatorTotal, InOperatorName);
	TArray<TArray<FNeuralTensor>> TensorsCopy = InTensors;
	TArray<TArray<FNeuralTensor*>> InputTensors({ {&TensorsCopy[0][0]}, {&TensorsCopy[1][0]}, {&TensorsCopy[2][0]}, {&TensorsCopy[3][0]}, {&TensorsCopy[4][0]} });
	TArray<TArray<FNeuralTensor*>> OutputTensors({ TArray<FNeuralTensor*>({/*Inlined*/}), {&TensorsCopy[1][1]}, TArray<FNeuralTensor*>({/*Inlined*/}), {&TensorsCopy[3][1]},
		TArray<FNeuralTensor*>({/*Inlined*/}) });
	TArray<TSharedPtr<FNeuralOperator>> Operators({ MakeShared<TNeuralOperatorChild>(/*bIsInlinedTensor*/true), MakeShared<TNeuralOperatorChild>(false), MakeShared<TNeuralOperatorChild>(true),
		MakeShared<TNeuralOperatorChild>(false), MakeShared<TNeuralOperatorChild>(true) });
	TestOperator(InOutNetworkInferenceQAAsset, TensorsCopy, InputTensors, OutputTensors, Operators, InOperatorName, InGroundTruthDirectory, InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false,
		/*bInShouldRunGPU*/true);
}

template<typename TNeuralOperatorChild>
void FOperatorTester::TestElementWise1AttributeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TCHAR* InOperatorName, const TArray<TArray<FNeuralTensor>>& InTensors, const float InZeroThreshold,
	const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d %s"), InOperatorCounter, InOperatorTotal, InOperatorName);
	TArray<TArray<FNeuralTensor>> TensorsCopy = InTensors;
	TensorsCopy += InTensors; // Append not implemented against itself
	TArray<TArray<FNeuralTensor*>> InputTensors({ {&TensorsCopy[0][0]}, {&TensorsCopy[1][0]}, {&TensorsCopy[2][0]}, {&TensorsCopy[3][0]}, {&TensorsCopy[4][0]},
		{&TensorsCopy[5][0]}, {&TensorsCopy[6][0]}, {&TensorsCopy[7][0]}, {&TensorsCopy[8][0]}, {&TensorsCopy[9][0]} });
	TArray<TArray<FNeuralTensor*>> OutputTensors({ TArray<FNeuralTensor*>({/*Inlined*/}), {&TensorsCopy[1][1]}, TArray<FNeuralTensor*>({/*Inlined*/}), {&TensorsCopy[3][1]},
		TArray<FNeuralTensor*>({/*Inlined*/}), TArray<FNeuralTensor*>({/*Inlined*/}), {&TensorsCopy[6][1]}, TArray<FNeuralTensor*>({/*Inlined*/}), {&TensorsCopy[8][1]},
		TArray<FNeuralTensor*>({/*Inlined*/}) });
	TArray<TSharedPtr<FNeuralOperator>> Operators({ MakeShared<TNeuralOperatorChild>(/*bIsInlinedTensor*/true, 1.f), MakeShared<TNeuralOperatorChild>(false, 20.f), MakeShared<TNeuralOperatorChild>(true, 0.5f),
		MakeShared<TNeuralOperatorChild>(false, 0.01f), MakeShared<TNeuralOperatorChild>(true, 0.f), MakeShared<TNeuralOperatorChild>(/*bIsInlinedTensor*/true, -1.f), MakeShared<TNeuralOperatorChild>(false, -20.f),
		MakeShared<TNeuralOperatorChild>(true, -0.5f), MakeShared<TNeuralOperatorChild>(false, -0.01f), MakeShared<TNeuralOperatorChild>(true, 1.2345f) });
	TestOperator(InOutNetworkInferenceQAAsset, TensorsCopy, InputTensors, OutputTensors, Operators, InOperatorName, InGroundTruthDirectory, InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false,
		/*bInShouldRunGPU*/true);
}

template<typename TNeuralOperatorChild>
void FOperatorTester::TestMultiDirectionalBroadcastingOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TCHAR* InOperatorName, const TArray<TArray<FNeuralTensor>>& InTensors, const float InZeroThreshold,
	const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d %s"), InOperatorCounter, InOperatorTotal, InOperatorName);

	const TSet<uint32> SetOperatorsAllInlined({ 0u, 1u });
	const TSet<uint32> SetOperatorAInlined({ 0u });
	const TSet<uint32> SetOperatorBInlined({ 1u });
	const TSet<uint32> SetNoOperatorsInlined;
	for (int32 TensorTestsIndex = 0; TensorTestsIndex < InTensors.Num(); ++TensorTestsIndex)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- %s #%d/%d"), InOperatorName, TensorTestsIndex + 1, InTensors.Num());
		TArray<TSharedPtr<FNeuralOperator>> Operators({ MakeShared<TNeuralOperatorChild>(SetOperatorsAllInlined), MakeShared<TNeuralOperatorChild>(SetOperatorAInlined),
			MakeShared<TNeuralOperatorChild>(SetOperatorBInlined), MakeShared<TNeuralOperatorChild>(SetNoOperatorsInlined) });
		// Input/output tensors
		TArray<TArray<FNeuralTensor>> MultiBroadcastTensors;
		MultiBroadcastTensors.Init(InTensors[TensorTestsIndex], Operators.Num());
		TArray<TArray<FNeuralTensor*>> InputTensors;
		TArray<TArray<FNeuralTensor*>> OutputTensors;
		for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
		{
			InputTensors.Push({ &MultiBroadcastTensors[OperatorIndex][0], &MultiBroadcastTensors[OperatorIndex][1] });
			OutputTensors.Push({ &MultiBroadcastTensors[OperatorIndex][2] });
		}
		// Run actual layer unit testing
		FOperatorTester::TestOperator(InOutNetworkInferenceQAAsset, MultiBroadcastTensors, InputTensors, OutputTensors, Operators, InOperatorName, InGroundTruthDirectory, InZeroThreshold,
			/*bShouldOperatorsProvideSameResult*/true, /*bShouldRunGPU*/true, (TensorTestsIndex + 1 == InTensors.Num()));
	}
}
