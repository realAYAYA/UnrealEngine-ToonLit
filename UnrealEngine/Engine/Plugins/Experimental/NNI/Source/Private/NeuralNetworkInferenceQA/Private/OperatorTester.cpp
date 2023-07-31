// Copyright Epic Games, Inc. All Rights Reserved.

#include "OperatorTester.h"
#include "NeuralNetworkInferenceQAUtils.h"
#include "NeuralOperators.h"
#include "Misc/Paths.h"



/* FOperatorTester static public functions
 *****************************************************************************/

void FOperatorTester::TestOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, TArray<TArray<FNeuralTensor>>& InOutTensors,
	TArray<TArray<FNeuralTensor*>>& InOutInputTensors, TArray<TArray<FNeuralTensor*>>& InOutOutputTensors,
	TArray<TSharedPtr<FNeuralOperator>>& InOutOperators, const FString& InOperatorName, const FString& InGroundTruthDirectory,
	const float InZeroThreshold, const bool bInShouldOperatorsProvideSameResult, const bool bInShouldRunGPU,
	const bool bInShouldRunFinalStringVsIOTest)
{
	// Sanity checks
	if (!InOutNetworkInferenceQAAsset)
	{
		ensureMsgf(false, TEXT("InOutNetworkInferenceQAAsset was nullptr!"));
		return;
	}
	else if (InOutInputTensors.Num() != InOutOperators.Num())
	{
		ensureMsgf(false, TEXT("InOutInputTensors.Num() == InOutOperators.Num(): %d vs. %d"), InOutInputTensors.Num(), InOutOperators.Num());
		return;
	}
	if (!bInShouldRunGPU)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("FOperatorTester::TestOperator(): bInShouldRunGPU is false, this should only happen for"
			" very early deployment of new operators."));
	}
	// Run test on each input FNeuralTensor
	FString Index0StringIfShouldProvideSameResult;
	for (int32 Index = 0; Index < InOutInputTensors.Num(); ++Index)
	{
		//UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("------------------------- Subtest #%d/%d"), Index+1, InOutInputTensors.Num());
		TSharedPtr<FNeuralOperator>& Operator = InOutOperators[Index];
		const TArray<FNeuralTensor*>& InputTensors = InOutInputTensors[Index];
		if (InputTensors.IsEmpty())
		{
			ensureMsgf(false, TEXT("InputTensors.Num() is 0 but should be > 0."));
			return;
		}
		// Operator array
		const int32 InlinedTensorIndex = Operator->SetInputTensorsAndGetInlinedIndex(InputTensors);
		// Tensor manager
		const TArray<FNeuralTensor*>& OutputTensors = (InlinedTensorIndex > -1
			? TArray<FNeuralTensor*>({ InputTensors[InlinedTensorIndex] }) : InOutOutputTensors[Index]);
		if (OutputTensors.IsEmpty())
		{
			ensureMsgf(false, TEXT("OutputTensors.Num() is 0 but should be > 0."));
			return;
		}
		// Operator array
		if (InlinedTensorIndex < 0)
		{
			Operator->SetOutputTensors(InOutOutputTensors[Index]);
		}
		// Create and add auxiliary tensors for that operator (if any)
		const int64 NumberAuxiliaryTensors = Operator->GetNumberAuxiliaryTensors();
		if (NumberAuxiliaryTensors > 0)
		{
			TArray<FNeuralTensor*> AuxiliaryOperatorTensors;
			for (int64 AuxiliaryTensorIndex = 0; AuxiliaryTensorIndex < Operator->GetNumberAuxiliaryTensors(); ++AuxiliaryTensorIndex)
			{
				const FString AuxiliaryTensorName = Operator->GetName() + TEXT("-Aux") + FString::FromInt(AuxiliaryTensorIndex);
				InOutTensors[Index].Push(FNeuralTensor(ENeuralDataType::None, TArray<int64>({}),
					AuxiliaryTensorName, ENeuralTensorType::Generic)); // ENeuralTensorType can be modify by FNeuralOperator::Configure()
				AuxiliaryOperatorTensors.Push(&InOutTensors[Index].Last());
			}
			Operator->SetAuxiliaryTensors(AuxiliaryOperatorTensors);
		}
		// Configure operator
		if (!Operator->Configure())
		{
			ensureMsgf(false, TEXT("Operator %s could not be configured."), *Operator->GetName());
			return;
		}
		// Network tests
		UNeuralNetwork* Network = NewObject<UNeuralNetwork>((UObject*)GetTransientPackage(), UNeuralNetwork::StaticClass());
		if (!Network)
		{
			ensureMsgf(false, TEXT("Network is nullptr."));
			return;
		}
		if (!Network->Load(InOutTensors[Index], InputTensors, OutputTensors, { Operator }))
		{
			ensureMsgf(false, TEXT("Network could not be loaded."));
			return;
		}
		// GPU vs. CPU test (if enabled)
		if (bInShouldRunGPU)
		{
			NetworkGPUvsCPU(Network, InlinedTensorIndex > -1, InZeroThreshold);
		}
		Network->SetDeviceType(ENeuralDeviceType::CPU);
		if (InlinedTensorIndex > -1)
		{
			NetworkInputVsOutput(Network, InlinedTensorIndex);
		}
		// Test against GT saved on disk
		// Case a) Only saving first test, the others should match the first test results
		if (bInShouldOperatorsProvideSameResult)
		{
			if (Index == 0)
			{
				InOutNetworkInferenceQAAsset->RunAndAddTest(Network, InOperatorName);
				Index0StringIfShouldProvideSameResult = FNeuralNetworkInferenceQAOperatorAsset::RunNetworkCPUAndGetString(Network);
			}
			else
			{
				// Sanity check
				const FString OutputTensorsAsString = FNeuralNetworkInferenceQAOperatorAsset::RunNetworkCPUAndGetString(Network);
				if (Index0StringIfShouldProvideSameResult != OutputTensorsAsString)
				{
					UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("Index0StringIfShouldProvideSameResult:\n%s"),
						*Index0StringIfShouldProvideSameResult);
					UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("OutputTensorsAsString:\n%s"), *OutputTensorsAsString);
					ensureMsgf(false, TEXT("Subtest #%d did not match subtest #0"), Index);
				}
			}
		}
		// Case b) Saving each sub-iteration
		else
		{
			InOutNetworkInferenceQAAsset->RunAndAddTest(Network, InOperatorName);
		}
	}
}

void FOperatorTester::TestBatchNormalizationOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset,
	const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataSmall,
	const TMap<int32, TArray<float>>& InInputTensorDataPos, const float InZeroThreshold, const FString& InGroundTruthDirectory,
	const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FBatchNormalizationOperator"), InOperatorCounter, InOperatorTotal);
	// Inout tensors (X != Mean to avoid Output = Bias; Variance positive to avoid NaN)
	TArray<TArray<FNeuralTensor>> Tensors({
		// ----------------------------------------------------------------- ONNX -----------------------------------------------------------------
		// https://github.com/onnx/onnx/blob/master/docs/TestCoverage.md#BatchNormalization
		{FNeuralTensor(TArray<float>({-1,0,1, 2,3,4}), TArray<int64>({1,2,1,3})),
			FNeuralTensor(TArray<float>({1, 1.5}), TArray<int64>({2})),
			FNeuralTensor(TArray<float>({0, 1}), TArray<int64>({2})),
			FNeuralTensor(TArray<float>({0, 3}), TArray<int64>({2})),
			FNeuralTensor(TArray<float>({1, 1.5}), TArray<int64>({2})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,1,3})}, // [-0.999995 0.0 0.999995; -0.22474074 1.0 2.2247407]
		{FNeuralTensor(InInputTensorData.FindChecked(120), TArray<int64>({2,3,4,5})),
			FNeuralTensor(InInputTensorData.FindChecked(3), TArray<int64>({3})),
			FNeuralTensor(InInputTensorData.FindChecked(3), TArray<int64>({3})),
			FNeuralTensor(InInputTensorData.FindChecked(3), TArray<int64>({3})),
			FNeuralTensor(InInputTensorDataPos.FindChecked(3), TArray<int64>({3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,1,3})},
		// Scalars
		{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorData.FindChecked(1)),
			FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorDataSmall.FindChecked(1)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(1)), FNeuralTensor(ENeuralDataType::Float, 1)},
		// Scalars
		{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorData.FindChecked(1)),
			FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorDataSmall.FindChecked(1)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(1)), FNeuralTensor(ENeuralDataType::Float, 1)},
		// 1-D
		{FNeuralTensor(InInputTensorData.FindChecked(10)), FNeuralTensor(InInputTensorData.FindChecked(1)),
			FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorDataSmall.FindChecked(1)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(1)), FNeuralTensor(ENeuralDataType::Float, 10)},
		// 4-D with 1 batch and 1 channel
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1, 1, 5, 20})), FNeuralTensor(InInputTensorData.FindChecked(1)),
			FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorDataSmall.FindChecked(1)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(1)), FNeuralTensor(ENeuralDataType::Float, {1, 1, 5, 20})},
		// 4-D with 1 channel
		{FNeuralTensor(InInputTensorData.FindChecked(150), TArray<int64>({5, 1, 6, 5})), FNeuralTensor(InInputTensorData.FindChecked(1)),
			FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorDataSmall.FindChecked(1)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(1)), FNeuralTensor(ENeuralDataType::Float, {5, 1, 6, 5})},
		// 4-D with 1 batch
		{FNeuralTensor(InInputTensorData.FindChecked(150), TArray<int64>({1, 5, 6, 5})), FNeuralTensor(InInputTensorData.FindChecked(5)),
			FNeuralTensor(InInputTensorData.FindChecked(5)), FNeuralTensor(InInputTensorDataSmall.FindChecked(5)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(5)), FNeuralTensor(ENeuralDataType::Float, {1, 5, 6, 5})},
		// 4-D
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({3, 10, 5, 2})), FNeuralTensor(InInputTensorData.FindChecked(10)),
			FNeuralTensor(InInputTensorData.FindChecked(10)), FNeuralTensor(InInputTensorDataSmall.FindChecked(10)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(10)), FNeuralTensor(ENeuralDataType::Float, {3, 10, 5, 2})},
		// 2-D
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({6, 3})), FNeuralTensor(InInputTensorData.FindChecked(3)),
			FNeuralTensor(InInputTensorData.FindChecked(3)), FNeuralTensor(InInputTensorDataSmall.FindChecked(3)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(3)), FNeuralTensor(ENeuralDataType::Float, {6, 3})},
		// 3-D
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({3, 5, 2})), FNeuralTensor(InInputTensorData.FindChecked(5)),
			FNeuralTensor(InInputTensorData.FindChecked(5)), FNeuralTensor(InInputTensorDataSmall.FindChecked(5)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(5)), FNeuralTensor(ENeuralDataType::Float, {3, 5, 2})},
		// 5-D
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({2, 3, 5, 5, 2})), FNeuralTensor(InInputTensorData.FindChecked(3)),
			FNeuralTensor(InInputTensorData.FindChecked(3)), FNeuralTensor(InInputTensorDataSmall.FindChecked(3)),
			FNeuralTensor(InInputTensorDataPos.FindChecked(3)), FNeuralTensor(ENeuralDataType::Float, {2, 3, 5, 5, 2})}
		});
	TArray<TArray<FNeuralTensor*>> InputTensors;
	TArray<TArray<FNeuralTensor*>> OutputTensors;
	TArray<TSharedPtr<FNeuralOperator>> Operators;
	for (int32 TensorIndex = 0; TensorIndex < Tensors.Num(); ++TensorIndex)
	{
		InputTensors.Push({ &Tensors[TensorIndex][0], &Tensors[TensorIndex][1], &Tensors[TensorIndex][2], &Tensors[TensorIndex][3],
			&Tensors[TensorIndex][4] });
		OutputTensors.Push(TensorIndex % 2 == 0 ? TArray<FNeuralTensor*>({ &Tensors[TensorIndex][5] }) : TArray<FNeuralTensor*>({/*Inlined*/ }));
		Operators.Push(MakeShared<FBatchNormalizationOperator>(OutputTensors[TensorIndex].Num() == 0,
			/*Epsilon*/(TensorIndex == 1 ? 1e-2 : TensorIndex == 3 ? 10 : TensorIndex == 6 ? 1 : 1e-5),
			/*Momentum*/(TensorIndex+1.f)/Tensors.Num()));
	}
	TestOperator(InOutNetworkInferenceQAAsset, Tensors, InputTensors, OutputTensors, Operators, TEXT("FBatchNormalizationOperator"),
		InGroundTruthDirectory, InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false, /*bInShouldRunGPU*/true);
}

void FOperatorTester::TestGemmOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset,
	const TMap<int32, TArray<float>>& InInputTensorData, const float InZeroThreshold, const FString& InGroundTruthDirectory,
	const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FGemmOperator"), InOperatorCounter, InOperatorTotal);
	// Inout tensors
	const TArray<TArray<FNeuralTensor>> GemmABCOutputTensorTests({
		// Scalars
		{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(InInputTensorData.FindChecked(1)),
			FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(ENeuralDataType::Float, 1)},
		// 1-D
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({50, 6})),
			FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({6, 3})),
			FNeuralTensor(InInputTensorData.FindChecked(150), TArray<int64>({50, 3})),
			FNeuralTensor(ENeuralDataType::Float, TArray<int64>({50, 3}))},
		// 1-D, C size = [(1), 3], the next 2 should give the exact same results
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({50, 6})),
			FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({6, 3})),
			FNeuralTensor(InInputTensorData.FindChecked(3), TArray<int64>({1, 3})),
			FNeuralTensor(ENeuralDataType::Float, TArray<int64>({50, 3}))}, // C size = [1, 3]
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({50, 6})),
			FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({6, 3})),
			FNeuralTensor(InInputTensorData.FindChecked(3)), FNeuralTensor(ENeuralDataType::Float, TArray<int64>({50, 3}))}, // C size = [3]
		// 1-D, C size = [50, 1]
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({50, 6})),
			FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({6, 3})),
			FNeuralTensor(InInputTensorData.FindChecked(50), TArray<int64>({50, 1})),
			FNeuralTensor(ENeuralDataType::Float, TArray<int64>({50, 3}))}
	});
	for (int32 TensorTestsIndex = 0; TensorTestsIndex < GemmABCOutputTensorTests.Num(); ++TensorTestsIndex)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- FGemmOperator #%d/%d"),
			TensorTestsIndex + 1, GemmABCOutputTensorTests.Num());
		TArray<TSharedPtr<FNeuralOperator>> Operators({
			// A, B
			MakeShared<FGemmOperator>(/*Alpha*/-0.5, /*Beta*/-10, /*bTransA*/false, /*bTransB*/false),// = AB + C
			MakeShared<FGemmOperator>(/*Alpha*/10, /*Beta*/0, /*bTransA*/false, /*bTransB*/false),    // = AB
			MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/0.1, /*bTransA*/false, /*bTransB*/false),   // = C
			MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/0, /*bTransA*/false, /*bTransB*/false),     // = 0
		});
		// 2nd and 3rd set of tests are mostly for C, so we can ignore bTransA/bTransB tests.
		if (TensorTestsIndex < 2)
		{
			Operators.Append({
				// A', B
				MakeShared<FGemmOperator>(/*Alpha*/1, /*Beta*/1, /*bTransA*/true, /*bTransB*/false),      // = A'B + C
				MakeShared<FGemmOperator>(/*Alpha*/0.5f, /*Beta*/0, /*bTransA*/true, /*bTransB*/false),   // = 0.5A'B
				MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/0.5f, /*bTransA*/true, /*bTransB*/false),   // = 0.5C
				MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/0, /*bTransA*/true, /*bTransB*/false),      // = 0
				// A, B'
				MakeShared<FGemmOperator>(/*Alpha*/1, /*Beta*/1, /*bTransA*/false, /*bTransB*/true),      // = AB' + C
				MakeShared<FGemmOperator>(/*Alpha*/10, /*Beta*/0, /*bTransA*/false, /*bTransB*/true),     // = 10AB'
				MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/10, /*bTransA*/false, /*bTransB*/true),     // = 10C
				MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/0, /*bTransA*/false, /*bTransB*/true),      // = 0
				// A', B'
				MakeShared<FGemmOperator>(/*Alpha*/1, /*Beta*/1, /*bTransA*/true, /*bTransB*/true),       // = A'B' + C
				MakeShared<FGemmOperator>(/*Alpha*/-5, /*Beta*/0, /*bTransA*/true, /*bTransB*/true),      // = -5A'B'
				MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/-5, /*bTransA*/true, /*bTransB*/true),      // = -5C
				MakeShared<FGemmOperator>(/*Alpha*/0, /*Beta*/0, /*bTransA*/true, /*bTransB*/true)        // = 0
			});
		}
		TArray<TArray<FNeuralTensor>> GemmABCOutputTensors;
		GemmABCOutputTensors.Init(GemmABCOutputTensorTests[TensorTestsIndex], Operators.Num());
		// Input/output tensors
		TArray<TArray<FNeuralTensor*>> InputTensors;
		TArray<TArray<FNeuralTensor*>> OutputTensors;
		for (int32 InoutIndex = 0; InoutIndex < Operators.Num(); ++InoutIndex)
		{
			// Disable C for half of the cases where Beta==0 (same result)
			if (/*Beta == 0*/InoutIndex % 2 == 1 && InoutIndex > 8)
			{
				InputTensors.Push({ &GemmABCOutputTensors[InoutIndex][0], &GemmABCOutputTensors[InoutIndex][1] });
			}
			else
			{
				InputTensors.Push({ &GemmABCOutputTensors[InoutIndex][0], &GemmABCOutputTensors[InoutIndex][1],
					&GemmABCOutputTensors[InoutIndex][2] });
			}
			// Transpose FNeuralTensor if bTransA or bTransB
			if ((InoutIndex >= 4 && InoutIndex < 8) || InoutIndex >= 12) // bTransA?
			{
				if (!InputTensors.Last()[0]->Transpose())
				{
					ensureMsgf(false, TEXT("FNeuralTensor::Transpose() failed."));
				}
			}
			if (InoutIndex >= 8) // bTransB?
			{
				if (!InputTensors.Last()[1]->Transpose())
				{
					ensureMsgf(false, TEXT("FNeuralTensor::Transpose() failed."));
				}
			}
			OutputTensors.Push({ &GemmABCOutputTensors[InoutIndex][3] });
		}
		// Run actual layer unit testing
		TestOperator(InOutNetworkInferenceQAAsset, GemmABCOutputTensors, InputTensors, OutputTensors, Operators,
			/*FileName*/TEXT("FGemmOperator"), InGroundTruthDirectory, InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false,
			/*bShouldRunGPU*/true, (TensorTestsIndex+1 == GemmABCOutputTensorTests.Num()));
	}
}

void FOperatorTester::TestConvOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset,
	const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataSmall, const float InZeroThreshold,
	const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FConvOperator"), InOperatorCounter, InOperatorTotal);
	// Inout tensors
	TArray<TArray<FNeuralTensor>> Tensors({
		// ----------------------------------------------------------------- ONNX -----------------------------------------------------------------
		// https://github.com/onnx/onnx/blob/master/docs/TestCoverage.md#Conv
		// conv
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24}), TArray<int64>({1,1,5,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5}), // [[[[12.0 21.0 27.0 33.0 24.0], [33.0 54.0 63.0 72.0 51.0], [63.0 99.0 108.0 117.0 81.0], [93.0 144.0 153.0 162.0 111.0], [72.0 111.0 117.0 123.0 84.0]]]]
			FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({1,1,1,1}))},
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24}), TArray<int64>({1,1,5,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,3})}, // [[[[54.0 63.0 72.0], [99.0 108.0 117.0], [144.0 153.0 162.0]]]]
		// conv_with_autopad_same
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24}), TArray<int64>({1,1,5,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,3}), // [[[[12.0 27.0 24.0], [63.0 108.0 81.0], [72.0 117.0 84.0]]]]
			FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)})), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2}))},
		// conv_with_strides
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24, 25,26,27,28,29, 30,31,32,33,34}), TArray<int64>({1,1,7,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,4,3}), // [[[[12.0 27.0 24.0], [63.0 108.0 81.0], [123.0 198.0 141.0], [112.0 177.0 124.0]]]]
			FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({1,1, 1,1})),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2}))},
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24, 25,26,27,28,29, 30,31,32,33,34}), TArray<int64>({1,1,7,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,2}), // [[[[54.0 72.0], [144.0 162.0], [234.0 252.0]]]]
			FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2}))},
		// Output size (thus solution) in onnx website is wrong. Replaced Y size to [3, 2]
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24, 25,26,27,28,29, 30,31,32,33,34}), TArray<int64>({1,1,7,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,2}), // [[[[12.0 27.0], [63.0 108.0], [123.0 198.0]]]]
			FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({1,0, 1,0})),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2}))},
		// ----------------------------------------------------------------- 1D convolutions -----------------------------------------------------------------
		// X={1,1,1}, W={1,1,1}, B={1}, Y={1,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,9}, W={1,1,5}, B={}, Y={1,1,5}
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5})},
		// X={1,3,27}, W={1,3,5}, B={}, Y={1,1,23}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,3,27})), FNeuralTensor(InInputTensorDataSmall.FindChecked(15), TArray<int64>({1,3,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,23})},
		// X={1,3,27}, W={2,3,5}, B={}, Y={1,2,23}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,3,27})), FNeuralTensor(InInputTensorDataSmall.FindChecked(30), TArray<int64>({2,3,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,23})},
		// X={5,3,20}, W={2,3,5}, B={2}, Y={5,2,16}
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({5,3,20})), FNeuralTensor(InInputTensorDataSmall.FindChecked(30), TArray<int64>({2,3,5})),
			FNeuralTensor(ENeuralDataType::Float, {5,2,16}), FNeuralTensor(InInputTensorDataSmall.FindChecked(2))},
		// ----------------------------------------------------------------- 2D convolutions -----------------------------------------------------------------
		// X={1,1,1,1}, W={1,1,1,1}, B={}, Y={1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1})},
		// X={1,1,1,9}, W={1,1,1,5}, B={}, Y={1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,5})},
		// X={5,3,1,20}, W={2,3,1,5}, B={}, Y={5,2,1,16} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({5,3,1,20})), FNeuralTensor(InInputTensorDataSmall.FindChecked(30), TArray<int64>({2,3,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {5,2,1,16})},
		// X={1,1,1,1}, W={1,1,1,1}, B={1}, Y={1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,5,10}, W={1,1,3,3}, B={}, Y={1,1,3,8}
		{FNeuralTensor(InInputTensorData.FindChecked(50), TArray<int64>({1,1,5,10})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,8})},
		// X={1,1,5,10}, W={1,1,3,3}, B={1}, Y={1,1,3,8}
		{FNeuralTensor(InInputTensorData.FindChecked(50), TArray<int64>({1,1,5,10})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,8}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={2,1,1,1}, W={5,1,1,1}, B={}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(2), TArray<int64>({2,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({5,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,5,1,1})},
		// X={2,1,1,1}, W={5,1,1,1}, B={5}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(2), TArray<int64>({2,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({5,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,5,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(5))},
		// X={2,3,1,1}, W={5,3,1,1}, B={}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(6), TArray<int64>({2,3,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(15), TArray<int64>({5,3,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,5,1,1})},
		// X={2,3,1,1}, W={5,3,1,1}, B={5}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(6), TArray<int64>({2,3,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(15), TArray<int64>({5,3,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,5,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(5))},
		// ----------------------------------------------------------------- 3D convolutions -----------------------------------------------------------------
		// X={1,1,1,1,9}, W={1,1,1,1,5}, B={1}, Y={1,1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,1,9,1}, W={1,1,1,5,1}, B={}, Y={1,1,1,5,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,1,9,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,5,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,5,1})},
		// X={1,1,1,1,1}, W={1,1,1,1,1}, B={}, Y={1,1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1})},
		// X={1,1,6,5,5}, W={1,1,3,3,3}, B={}, Y={1,1,4,3,3}
		{FNeuralTensor(InInputTensorData.FindChecked(150), TArray<int64>({1,1,6,5,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,4,3,3})},
		// X={2,2,3,5,5}, W={9,2,3,3,3}, B={9}, Y={2,9,1,3,3}
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({2,2,3,5,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({9,2,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {2,9,1,3,3}), FNeuralTensor(InInputTensorDataSmall.FindChecked(9))},
		// ----------------------------------------------------------------- 4D convolutions -----------------------------------------------------------------
		// X={1,1,1,1,1,1}, W={1,1,1,1,1,1}, B={1}, Y={1,1,1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,1,1,1,9}, W={1,1,1,1,1,5}, B={}, Y={1,1,1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,1,1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1,5})},
		// X={1,1,3,4,5,5}, W={1,1,3,3,3,3}, B={}, Y={1,1,1,2,3,3}
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({1,1,3,4,5,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(81), TArray<int64>({1,1,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,2,3,3})},
		// X={3,3,4,5,3,5}, W={2,3,3,3,3,3}, B={2}, Y={3,2,2,3,1,3}
		{FNeuralTensor(InInputTensorData.FindChecked(2700), TArray<int64>({3,3,4,5,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({2,3,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {3,2,2,3,1,3}), FNeuralTensor(InInputTensorDataSmall.FindChecked(2))},
		// ----------------------------------------------------------------- 8D convolutions -----------------------------------------------------------------
		// X={1,1, 1,1,1,1,1,1,1,1}, W={1,1, 1,1,1,1,1,1,1,1}, B={1}, Y={1,1, 1,1,1,1,1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1, 1,1,1,1,1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1, 1,1,1,1,1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1, 1,1,1,1,1,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1, 1,1,1,1,1,1,1,9}, W={1,1, 1,1,1,1,1,1,1,5}, B={}, Y={1,1, 1,1,1,1,1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1, 1,1,1,1,1,1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1, 1,1,1,1,1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1, 1,1,1,1,1,1,1,5})},
		// X={1,1, 1,1,1,1,3,4,5,5}, W={1,1, 1,1,1,1,3,3,3,3}, B={}, Y={1,1, 1,1,1,1,1,2,3,3} (replicates 4D example)
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({1,1, 1,1,1,1,3,4,5,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(81), TArray<int64>({1,1, 1,1,1,1,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1, 1,1,1,1,1,2,3,3})},
		// X={3,3, 1,1,1,1,4,5,3,5}, W={2,3, 1,1,1,1,3,3,3,3}, B={2}, Y={3,2, 1,1,1,1,2,3,1,3} (replicates 4D example)
		{FNeuralTensor(InInputTensorData.FindChecked(2700), TArray<int64>({3,3, 1,1,1,1,4,5,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({2,3, 1,1,1,1,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {3,2, 1,1,1,1,2,3,1,3}), FNeuralTensor(InInputTensorDataSmall.FindChecked(2))},
		// ----------------------------------------------------------------- Grouped convolutions -----------------------------------------------------------------
		// X={1,4,1}, W={8,4/2,1}, B={}, Y={1,8,1}, Group = 2
		{FNeuralTensor(TArray<float>({1,1,1,1}), TArray<int64>({1,4,1})),
			FNeuralTensor(TArray<float>({1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1}), TArray<int64>({8,4,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,8,1})},
		{FNeuralTensor(TArray<float>({1,1,1,1}), TArray<int64>({1,4,1})),
			FNeuralTensor(TArray<float>({1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1}), TArray<int64>({8,2,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,8,1}), FNeuralTensor(), /*Group*/FNeuralTensor(TArray<float>({2}))},
		// X={1,4,1}, W={8,4/2,1}, B={}, Y={1,8,1}, Group = 2
		{FNeuralTensor(InInputTensorData.FindChecked(4), TArray<int64>({1,4,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(16), TArray<int64>({8,2,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,8,1}), FNeuralTensor(), /*Group*/FNeuralTensor(TArray<float>({2}))},
		// X={1,9,9,6}, W={6,9/3,9,3}, B={}, Y={1,6,1,4}, Group = 3
		{FNeuralTensor(InInputTensorData.FindChecked(486), TArray<int64>({1,9,9,6})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({6,3,9,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,6,1,4}), FNeuralTensor(), /*Group*/FNeuralTensor(TArray<float>({3}))},
		// X={1,8,1,1,1}, W={4,8/4,1,1,1}, B={4}, Y={1,4,1,1,1}, Group = 4
		{FNeuralTensor(InInputTensorData.FindChecked(8), TArray<int64>({1,8,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(8), TArray<int64>({4,2,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,4,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(4)), /*Group*/FNeuralTensor(TArray<float>({4}))},
		// ----------------------------------------------------------------- AutoPad == SameLower/Upper -----------------------------------------------------------------
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                        AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,9}, W={1,1,9}, B={}, Y={1,1,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,9})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                        AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,9})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,9,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                              AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,9,9}, W={1,1,5,5}, B={}, Y={1,1,9,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(25), TArray<int64>({1,1,5,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                              AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(25), TArray<int64>({1,1,5,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,5,5,4}, W={1,1,3,3,3}, B={}, Y={1,1,5,5,4}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                                    AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,1,1,5,5,4}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,5,5,4}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                                    AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4,1,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={2,3,5,2,3,3,5}, W={2,3,5,2,1,1,5}, B={}, Y={2,2,5,2,3,3,5}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(2700), TArray<int64>({2,3,5,2,3,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(300), TArray<int64>({2,3,5,2,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {2,2,5,2,3,3,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//																  AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(2700), TArray<int64>({2,3,5,2,3,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(300), TArray<int64>({2,3,5,2,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {2,2,5,2,3,3,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// ----------------------------------------------------------------- AutoPad = EAutoPad::NotSet (i.e., manual padding) -----------------------------------------------------------------
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,16}
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,16}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,10}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,12,17}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,12,17}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({5,0, 5,5}))},
		// X={1,1,5,5,4}, W={1,1,3,3,3}, B={}, Y={1,1,4,8,11}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,4,8,11}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,1, 2,3, 4,5}))},
		// X={1,1,1,1,5,5,4}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,2,6,12,10,5}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,2,6,12,10,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,1, 2,3, 4,5, 4,3, 2,1}))},
		// ----------------------------------------------------------------- Dilations != 1 -----------------------------------------------------------------
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,3}
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({2}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,3,5}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({3,2}))},
		// X={1,1,5,4,5}, W={1,1,3,3,3}, B={}, Y={1,1,1,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,2,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({2,1,2}))},
		// X={1,1,1,1,5,4,5}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,1,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,1,1,5,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1,2,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({99,10,2,1,2}))},
		// ----------------------------------------------------------------- Strides != 1 -----------------------------------------------------------------
		// Required manual padding = 0 to avoid this unit tester to accidentally auto-trigger AutoPad == SameLower/Upper
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,2}
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,2}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({5}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,1,3}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,3}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({99,3}))},
		// X={1,1,5,4,5}, W={1,1,3,3,3}, B={}, Y={1,1,2,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,2,2,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,1,99}))},
		// X={1,1,1,1,5,4,5}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,2,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,1,1,5,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,2,2,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({1,50,2,1,99}))},
		// ----------------------------------------------------------------- Strides != 1, Multi-channel -----------------------------------------------------------------
		// X={5,5,25}, W={2,5,9}, B={}, Y={5,2,9}
		{FNeuralTensor(InInputTensorData.FindChecked(625), TArray<int64>({5,5,25})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,9})),
			FNeuralTensor(ENeuralDataType::Float, {5,2,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2}))},
		// X={5,5,5,5}, W={2,5,3,3}, B={}, Y={5,2,2,2}
		{FNeuralTensor(InInputTensorData.FindChecked(625), TArray<int64>({5,5,5,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {5,2,2,2}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2}))},
		// X={5,5,5,5,1}, W={2,5,3,3,1}, B={}, Y={5,2,2,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(625), TArray<int64>({5,5,5,5,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,3,3,1})),
			FNeuralTensor(ENeuralDataType::Float, {5,2,2,2,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2,1}))},
		// X={5,5,5,5,1,1,1}, W={2,5,3,3,1,1,1}, B={}, Y={5,2,2,2,1,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(625), TArray<int64>({5,5,5,5,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,3,3,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {5,2,2,2,1,1,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2,1,1,1}))},
		// ----------------------------------------------------------------- Manual padding or AutoPad == SameLower/Upper, Dilations != 1, Strides != 1 -----------------------------------------------------------------
		// X={1,1,18}, W={1,1,4}, B={}, Y={1,1,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({1,1,18})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)})), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(TArray<float>({2})), /*Strides*/FNeuralTensor(TArray<float>({2}))},
		//										   AutoPad = EAutoPad::SameUpper || EAutoPad::SameLower
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({1,1,18})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)})), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(TArray<float>({2})), /*Strides*/FNeuralTensor(TArray<float>({2}))},
		// X={1,1,100}, W={1,1,3}, B={}, Y={1,1,21}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,100})), FNeuralTensor(InInputTensorDataSmall.FindChecked(3), TArray<int64>({1,1,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,21}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({10,5})),
			/*Dilations*/FNeuralTensor(TArray<float>({5})), /*Strides*/FNeuralTensor(TArray<float>({5}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,16,14}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,16,14}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({10,1,0,7})),
			/*Dilations*/FNeuralTensor(TArray<float>({2,1})), /*Strides*/FNeuralTensor(TArray<float>({1,1}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,4,5}
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({10,1,0,7})),
			/*Dilations*/FNeuralTensor(TArray<float>({2,1})), /*Strides*/FNeuralTensor(TArray<float>({4,3}))},
		// X={1,1,5,4,5}, W={1,1,3,3,3}, B={}, Y={1,1,5,4,5}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({10,6,1,12,15,0})),
			/*Dilations*/FNeuralTensor(TArray<float>({3,3,3})), /*Strides*/FNeuralTensor(TArray<float>({3,3,3}))},
		// X={1,1,1,1,5,4,5}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,5,4,5}
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,1,1,5,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,0,0,0,10,6,1,12,15,0})),
			/*Dilations*/FNeuralTensor(TArray<float>({8,5,3,3,3})), /*Strides*/FNeuralTensor(TArray<float>({2,9,3,3,3}))}
		});
	TArray<TArray<FNeuralTensor*>> InputTensors;
	TArray<TArray<FNeuralTensor*>> OutputTensors;
	TArray<TSharedPtr<FNeuralOperator>> Operators;
	for (int32 TensorIndex = 0; TensorIndex < Tensors.Num(); ++TensorIndex)
	{
		TArray<FNeuralTensor>& TensorsIndex = Tensors[TensorIndex];
		if (TensorsIndex.Num() > 3 && TensorsIndex[3].Num() > 0)
		{
			InputTensors.Push({ &TensorsIndex[0], &TensorsIndex[1], &TensorsIndex[3] });
		}
		else
		{
			InputTensors.Push({ &TensorsIndex[0], &TensorsIndex[1] });
		}
		OutputTensors.Push({ &TensorsIndex[2] });
		// Set Group - Hacky but simple way of getting group value
		//const int64 Group = (TensorsIndex.Num() > 4 && TensorsIndex[4].Num() > 0 ? FMath::RoundToInt(TensorsIndex[4].At<float>(0)) : 1);
		const TArray<int64> GroupArray = FloatTensorToIntArray(TensorsIndex, 4);
		if (GroupArray.Num() > 1)
		{
			ensureMsgf(false, TEXT("GroupArray.Num() = %d == 0 | 1 failed."), GroupArray.Num());
			return;
		}
		const int64 Group = (GroupArray.Num() > 0 ? GroupArray[0] : 1);
		// Set FConvBaseOperator::EAutoPad - Hacky but simple way of getting this TArray
		FConvBaseOperator::EAutoPad AutoPad = FConvBaseOperator::EAutoPad::NotSet;
		const TArray<int64> AutoPadArray = FloatTensorToIntArray(TensorsIndex, 5);
		if (AutoPadArray.Num() > 0)
		{
			if (AutoPadArray.Num() != 1)
			{
				ensureMsgf(false, TEXT("AutoPadArray should always be 1."));
				return;
			}
			AutoPad = FConvBaseOperator::EAutoPad(AutoPadArray[0]);
		}
		// Set Pads - Hacky but simple way of getting this TArray
		const TArray<int64> Pads = FloatTensorToIntArray(TensorsIndex, 6);
		// Set Dilations - Hacky but simple way of getting this TArray
		const TArray<int64> Dilations = FloatTensorToIntArray(TensorsIndex, 7);
		// Set Strides - Hacky but simple way of getting this TArray
		const TArray<int64> Strides = FloatTensorToIntArray(TensorsIndex, 8);
		// Set FConvBaseOperator::EAutoPad - Hacky but simple way of getting this TArray
		// Create and push FConvOperator
		Operators.Push(MakeShared<FConvOperator>(AutoPad, Dilations, Group, /*KernelShape*/TArray<int64>(), Pads, Strides));
		// Remove all non-required/auxiliary tensors from InputTensors to keep it in X, W, and B (if B exists). Otherwise, those would be used by
		// the network and moved into GPU memory
		while (Tensors[TensorIndex].Num() > 4 || Tensors[TensorIndex].Last().Num() < 1)
		{
			Tensors[TensorIndex].Pop();
		}
		// TensorsIndex is no longer valid after this while loop!
	}
	TestOperator(InOutNetworkInferenceQAAsset, Tensors, InputTensors, OutputTensors, Operators, TEXT("FConvOperator"), InGroundTruthDirectory,
		InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false, /*bInShouldRunGPU*/true);
}

void FOperatorTester::TestConvTransposeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset, const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataSmall,
	const float InZeroThreshold, const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FConvTransposeOperator"), InOperatorCounter, InOperatorTotal);
	// Inout tensors
	TArray<TArray<FNeuralTensor>> Tensors({
		// ----------------------------------------------------------------- ONNX -----------------------------------------------------------------
		// https://github.com/onnx/onnx/blob/master/docs/TestCoverage.md#ConvTranspose
		// convtranspose_1d
		{FNeuralTensor(TArray<float>({0,1,2}), TArray<int64>({1,1,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1}), TArray<int64>({1,2,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,5})}, // [[[0.0 1.0 3.0 3.0 2.0], [0.0 1.0 3.0 3.0 2.0]]]
		// convtranspose
		{FNeuralTensor(TArray<float>({0,1,2, 3,4,5, 6,7,8}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1,   1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,5,5})}, // [[[[0.0 1.0 3.0 3.0 2.0], [3.0 8.0 15.0 12.0 7.0], [9.0 21.0 36.0 27.0 15.0], [9.0 20.0 33.0 24.0 13.0], [6.0 13.0 21.0 15.0 8.0]], [repeated]]
		// convtranspose_3d
		{FNeuralTensor(TArray<float>({0,1,2,3,4, 5,6,7,8,9, 10,11,12,13,14, 15,16,17,18,19, 20,21,22,23,24, 25,26,27,28,29, 30,31,32,33,34, 35,36,37,38,39, 40,41,42,43,44, 45,46,47,48,49, 50,51,52,53,54, 55,56,57,58,59}), TArray<int64>({1,1,3,4,5})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,5,6,7})},
		// convtranspose_attributes
		// Y = [[[[0.0 0.0 1.0 1.0 3.0 2.0 2.0 0.0], [0.0 0.0 1.0 1.0 3.0 2.0 2.0 0.0], [0.0 0.0 1.0 1.0 3.0 2.0 2.0 0.0], [3.0 3.0 7.0 4.0 9.0 5.0 5.0 0.0], [3.0 3.0 7.0 4.0 9.0 5.0 5.0 0.0], [3.0 3.0 7.0 4.0 9.0 5.0 5.0 0.0], [6.0 6.0 13.0 7.0 15.0 8.0 8.0 0.0],
		//        [6.0 6.0 13.0 7.0 15.0 8.0 8.0 0.0], [6.0 6.0 13.0 7.0 15.0 8.0 8.0 0.0], [0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0]], [[repeated]]
		{FNeuralTensor(TArray<float>({0,1,2, 3,4,5, 6,7,8}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1,   1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,10,8}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({3,2})), /*OutputPadding*/FNeuralTensor(), /*OutputShape*/FNeuralTensor(TArray<float>({10,8}))},
		{FNeuralTensor(TArray<float>({0,1,2, 3,4,5, 6,7,8}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1,   1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,10,8}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({3,2})), /*OutputPadding*/FNeuralTensor(TArray<float>({1,1})), /*OutputShape*/FNeuralTensor()},
		{FNeuralTensor(TArray<float>({0,1,2, 3,4,5, 6,7,8}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1,   1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,10,8}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({3,2})), /*OutputPadding*/FNeuralTensor(TArray<float>({1,1})), /*OutputShape*/FNeuralTensor(TArray<float>({10,8}))},
		// convtranspose_autopad_same
		// Y = [[[[0.0 0.0 1.0 1.0 3.0 2.0], [0.0 0.0 1.0 1.0 3.0 2.0], [3.0 3.0 8.0 5.0 12.0 7.0], [3.0 3.0 7.0 4.0 9.0 5.0], [9.0 9.0 20.0 11.0 24.0 13.0], [6.0 6.0 13.0 7.0 15.0 8.0]],
		//       [[0.0 0.0 1.0 1.0 3.0 2.0], [0.0 0.0 1.0 1.0 3.0 2.0], [3.0 3.0 8.0 5.0 12.0 7.0], [3.0 3.0 7.0 4.0 9.0 5.0], [9.0 9.0 20.0 11.0 24.0 13.0], [6.0 6.0 13.0 7.0 15.0 8.0]]]]
		{FNeuralTensor(TArray<float>({0,1,2, 3,4,5, 6,7,8}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1,   1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,6,6}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)})), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2})), /*OutputPadding*/FNeuralTensor(), /*OutputShape*/FNeuralTensor()},
		// convtranspose_dilations
		// Y = [[[[21.0 56.0 13.0 16.0 2.0], [63.0 35.0 67.0 10.0 14.0], [24.0 22.0 76.0 76.0 21.0], [9.0 5.0 88.0 45.0 63.0], [3.0 2.0 33.0 18.0 54.0]]]]
		{FNeuralTensor(TArray<float>({3,8,1, 9,5,7, 3,2,6}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({7,2, 1,9}), TArray<int64>({1,1,2,2})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(TArray<float>({2,2})), /*Strides*/FNeuralTensor(), /*OutputPadding*/FNeuralTensor(), /*OutputShape*/FNeuralTensor()},
		// convtranspose_pads
		// Output size (thus solution) in onnx website is wrong. Replaced Y size to [6, 4]
		// Y = [[[[0.0 1.0 1.0 3.0], [0.0 1.0 1.0 3.0], [3.0 7.0 4.0 9.0], [3.0 7.0 4.0 9.0], [3.0 7.0 4.0 9.0], [6.0 13.0 7.0 15.0]], [[0.0 1.0 1.0 3.0], [0.0 1.0 1.0 3.0], [3.0 7.0 4.0 9.0], [3.0 7.0 4.0 9.0], [3.0 7.0 4.0 9.0], [6.0 13.0 7.0 15.0]]]]
		{FNeuralTensor(TArray<float>({0,1,2, 3,4,5, 6,7,8}), TArray<int64>({1,1,3,3})),
			FNeuralTensor(TArray<float>({1,1,1, 1,1,1, 1,1,1,   1,1,1, 1,1,1, 1,1,1}), TArray<int64>({1,2,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,2,6,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({1,2,1,2})),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({3,2})), /*OutputPadding*/FNeuralTensor(), /*OutputShape*/FNeuralTensor()},
		// ----------------------------------------------------------------- 1D convolutions -----------------------------------------------------------------
		// X={1,1,1}, W={1,1,1}, B={1}, Y={1,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,9}, W={1,1,5}, B={}, Y={1,1,5}
		{FNeuralTensor(InInputTensorData.FindChecked(5), TArray<int64>({1,1,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9})},
		// X={1,3,27}, W={1,3,5}, B={}, Y={1,1,23}
		{FNeuralTensor(InInputTensorData.FindChecked(23), TArray<int64>({1,1,23})), FNeuralTensor(InInputTensorDataSmall.FindChecked(15), TArray<int64>({1,3,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,3,27})},
		// X={1,3,27}, W={2,3,5}, B={}, Y={1,2,23}
		{FNeuralTensor(InInputTensorData.FindChecked(46), TArray<int64>({1,2,23})), FNeuralTensor(InInputTensorDataSmall.FindChecked(30), TArray<int64>({2,3,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,3,27})},
		// X={5,3,20}, W={2,3,5}, B={3}, Y={5,2,16}
		{FNeuralTensor(InInputTensorData.FindChecked(160), TArray<int64>({5,2,16})), FNeuralTensor(InInputTensorDataSmall.FindChecked(30), TArray<int64>({2,3,5})),
			FNeuralTensor(ENeuralDataType::Float, {5,3,20}), FNeuralTensor(InInputTensorDataSmall.FindChecked(3))},
		// ----------------------------------------------------------------- 2D convolutions -----------------------------------------------------------------
		// X={1,1,1,1}, W={1,1,1,1}, B={}, Y={1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1})},
		// X={1,1,1,9}, W={1,1,1,5}, B={}, Y={1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(5), TArray<int64>({1,1,1,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,9})},
		// X={5,3,1,20}, W={2,3,1,5}, B={}, Y={5,2,1,16} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(160), TArray<int64>({5,2,1,16})), FNeuralTensor(InInputTensorDataSmall.FindChecked(30), TArray<int64>({2,3,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {5,3,1,20})},
		// X={1,1,1,1}, W={1,1,1,1}, B={1}, Y={1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,5,10}, W={1,1,3,3}, B={}, Y={1,1,3,8}
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({1,1,3,8})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,10})},
		// X={1,1,5,10}, W={1,1,3,3}, B={1}, Y={1,1,3,8}
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({1,1,3,8})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,10}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={2,1,1,1}, W={5,1,1,1}, B={}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(10), TArray<int64>({2,5,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({5,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,1,1,1})},
		// X={2,1,1,1}, W={5,1,1,1}, B={1}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(10), TArray<int64>({2,5,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({5,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={2,3,1,1}, W={5,3,1,1}, B={}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(10), TArray<int64>({2,5,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(15), TArray<int64>({5,3,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,3,1,1})},
		// X={2,3,1,1}, W={5,3,1,1}, B={3}, Y={2,5,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(10), TArray<int64>({2,5,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(15), TArray<int64>({5,3,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {2,3,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(3))},
		// ----------------------------------------------------------------- 3D convolutions -----------------------------------------------------------------
		// X={1,1,1,1,9}, W={1,1,1,1,5}, B={1}, Y={1,1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(5), TArray<int64>({1,1,1,1,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,9}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,1,9,1}, W={1,1,1,5,1}, B={}, Y={1,1,1,5,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(5), TArray<int64>({1,1,1,5,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,5,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,9,1})},
		// X={1,1,1,1,1}, W={1,1,1,1,1}, B={}, Y={1,1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1})},
		// X={1,1,6,5,5}, W={1,1,3,3,3}, B={}, Y={1,1,4,3,3}
		{FNeuralTensor(InInputTensorData.FindChecked(36), TArray<int64>({1,1,4,3,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,6,5,5})},
		// X={2,2,3,5,5}, W={9,2,3,3,3}, B={2}, Y={2,9,1,3,3}
		{FNeuralTensor(InInputTensorData.FindChecked(162), TArray<int64>({2,9,1,3,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({9,2,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {2,2,3,5,5}), FNeuralTensor(InInputTensorDataSmall.FindChecked(2))},
		// ----------------------------------------------------------------- 4D convolutions -----------------------------------------------------------------
		// X={1,1,1,1,1,1}, W={1,1,1,1,1,1}, B={1}, Y={1,1,1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1,1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1,1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1,1,1,1,9}, W={1,1,1,1,1,5}, B={}, Y={1,1,1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(5), TArray<int64>({1,1,1,1,1,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1,1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,1,9})},
		// X={1,1,3,4,5,5}, W={1,1,3,3,3,3}, B={}, Y={1,1,1,2,3,3}
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({1,1,1,2,3,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(81), TArray<int64>({1,1,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,3,4,5,5})},
		// X={3,3,4,5,3,5}, W={2,3,3,3,3,3}, B={3}, Y={3,2,2,3,1,3}
		{FNeuralTensor(InInputTensorData.FindChecked(108), TArray<int64>({3,2,2,3,1,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({2,3,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {3,3,4,5,3,5}), FNeuralTensor(InInputTensorDataSmall.FindChecked(3))},
		// ----------------------------------------------------------------- 8D convolutions -----------------------------------------------------------------
		// X={1,1, 1,1,1,1,1,1,1,1}, W={1,1, 1,1,1,1,1,1,1,1}, B={1}, Y={1,1, 1,1,1,1,1,1,1,1} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(1), TArray<int64>({1,1, 1,1,1,1,1,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(1), TArray<int64>({1,1, 1,1,1,1,1,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1, 1,1,1,1,1,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(1))},
		// X={1,1, 1,1,1,1,1,1,1,9}, W={1,1, 1,1,1,1,1,1,1,5}, B={}, Y={1,1, 1,1,1,1,1,1,1,5} (replicates 1D example)
		{FNeuralTensor(InInputTensorData.FindChecked(5), TArray<int64>({1,1, 1,1,1,1,1,1,1,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(5), TArray<int64>({1,1, 1,1,1,1,1,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1, 1,1,1,1,1,1,1,9})},
		// X={1,1, 1,1,1,1,3,4,5,5}, W={1,1, 1,1,1,1,3,3,3,3}, B={}, Y={1,1, 1,1,1,1,1,2,3,3} (replicates 4D example)
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({1,1, 1,1,1,1,1,2,3,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(81), TArray<int64>({1,1, 1,1,1,1,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1, 1,1,1,1,3,4,5,5})},
		// X={3,3, 1,1,1,1,4,5,3,5}, W={2,3, 1,1,1,1,3,3,3,3}, B={3}, Y={3,2, 1,1,1,1,2,3,1,3} (replicates 4D example)
		{FNeuralTensor(InInputTensorData.FindChecked(108), TArray<int64>({3,2, 1,1,1,1,2,3,1,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({2,3, 1,1,1,1,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {3,3, 1,1,1,1,4,5,3,5}), FNeuralTensor(InInputTensorDataSmall.FindChecked(3))},
		// ----------------------------------------------------------------- Grouped convolutions -----------------------------------------------------------------
		// X={1,4,1}, W={8,4/2,1}, B={}, Y={1,8,1}, Group = 2
		{FNeuralTensor(TArray<float>({1,1,1,1, 1,1,1,1}), TArray<int64>({1,8,1})),
			FNeuralTensor(TArray<float>({1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1}), TArray<int64>({8,4,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,4,1})},
		{FNeuralTensor(TArray<float>({1,1,1,1, 1,1,1,1}), TArray<int64>({1,8,1})),
			FNeuralTensor(TArray<float>({1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1}), TArray<int64>({8,2,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,4,1}), FNeuralTensor(), /*Group*/FNeuralTensor(TArray<float>({2}))},
		{FNeuralTensor(InInputTensorData.FindChecked(8), TArray<int64>({1,8,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(16), TArray<int64>({8,2,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,4,1}), FNeuralTensor(), /*Group*/FNeuralTensor(TArray<float>({2}))},
		// X={1,9,9,6}, W={6,9/3,9,3}, B={}, Y={1,6,1,4}, Group = 3
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({1,6,1,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(486), TArray<int64>({6,3,9,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,9,9,6}), FNeuralTensor(), /*Group*/FNeuralTensor(TArray<float>({3}))},
		// X={1,8,1,1,1}, W={4,8/4,1,1,1}, B={8}, Y={1,4,1,1,1}, Group = 4
		{FNeuralTensor(InInputTensorData.FindChecked(4), TArray<int64>({1,4,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(8), TArray<int64>({4,2,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,8,1,1,1}), FNeuralTensor(InInputTensorDataSmall.FindChecked(8)), /*Group*/FNeuralTensor(TArray<float>({4}))},
		// ----------------------------------------------------------------- AutoPad == SameLower/Upper -----------------------------------------------------------------
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                        AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)})) },
		// X={1,1,9}, W={1,1,9}, B={}, Y={1,1,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,9})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                        AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,9})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,9,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                              AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,9,9}, W={1,1,5,5}, B={}, Y={1,1,9,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(25), TArray<int64>({1,1,5,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                              AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(81), TArray<int64>({1,1,9,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(25), TArray<int64>({1,1,5,5})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,5,5,4}, W={1,1,3,3,3}, B={}, Y={1,1,5,5,4}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                                    AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={1,1,1,1,5,5,4}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,5,5,4}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,1,1,5,5,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//                                                    AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(100), TArray<int64>({1,1,5,5,4,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4,1,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// X={2,3,5,2,3,3,5}, W={2,3,5,2,1,1,5}, B={}, Y={2,2,5,2,3,3,5}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(1800), TArray<int64>({2,2,5,2,3,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(300), TArray<int64>({2,3,5,2,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {2,3,5,2,3,3,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameLower)}))},
		//																  AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(1800), TArray<int64>({2,2,5,2,3,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(300), TArray<int64>({2,3,5,2,1,1,5})),
			FNeuralTensor(ENeuralDataType::Float, {2,3,5,2,3,3,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(TArray<float>({float(FConvBaseOperator::EAutoPad::SameUpper)}))},
		// ----------------------------------------------------------------- AutoPad = EAutoPad::NotSet (i.e., manual padding) -----------------------------------------------------------------
		// X={1,1,9} + {0+3}, W={1,1,4}, B={}, Y={1,1,9}
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,3}))},
		// X={1,1,9,9} + {1+0,2+1}, W={1,1,3,3}, B={}, Y={1,1,8,10}
		{FNeuralTensor(InInputTensorData.FindChecked(80), TArray<int64>({1,1,8,10})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({1,0, 2,1}))},
		// X={1,1,5,5,4} + {0+1,2+2,2+0}, W={1,1,3,3,3}, B={}, Y={1,1,4,7,4}
		{FNeuralTensor(InInputTensorData.FindChecked(112), TArray<int64>({1,1,4,7,4})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,1, 2,2, 2,0}))},
		// X={1,1,1,1,5,5,4} + {2+2,2+2,0,1+2,2+1}, W={1,1,3,3,3,3,3}, B={}, Y={1,1,3,3,3,6,5}
		{FNeuralTensor(InInputTensorData.FindChecked(810), TArray<int64>({1,1,3,3,3,6,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(243), TArray<int64>({1,1,3,3,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,5,4}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({2,2, 2,2, 0,0, 1,2, 2,1}))},
		// ----------------------------------------------------------------- Dilations != 1 -----------------------------------------------------------------
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,3}
		{FNeuralTensor(InInputTensorData.FindChecked(3), TArray<int64>({1,1,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({2}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,3,5}
		{FNeuralTensor(InInputTensorData.FindChecked(15), TArray<int64>({1,1,3,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({3,2}))},
		// X={1,1,5,4,5}, W={1,1,3,3,3}, B={}, Y={1,1,1,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(2), TArray<int64>({1,1,1,2,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({2,1,2}))},
		// X={1,1,1,1,5,4,5}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,1,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(2), TArray<int64>({1,1,1,1,1,2,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(), /*Dilations*/FNeuralTensor(TArray<float>({99,10,2,1,2}))},
		// ----------------------------------------------------------------- Strides != 1 -----------------------------------------------------------------
		// Required manual padding = 0 to avoid this unit tester to accidentally auto-trigger AutoPad == SameLower/Upper
		// X={1,1,9}, W={1,1,4}, B={}, Y={1,1,2}
		{FNeuralTensor(InInputTensorData.FindChecked(2), TArray<int64>({1,1,2})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({5}))},
		// X={1,1,9,9}, W={1,1,3,3}, B={}, Y={1,1,1,3}
		{FNeuralTensor(InInputTensorData.FindChecked(3), TArray<int64>({1,1,1,3})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({7,3}))},
		// X={1,1,5,4,5}, W={1,1,3,3,3}, B={}, Y={1,1,2,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(4), TArray<int64>({1,1,2,2,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,1,3}))},
		// X={1,1,1,1,5,4,5}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,2,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(4), TArray<int64>({1,1,1,1,2,2,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({1,1,2,1,3}))},
		// ----------------------------------------------------------------- Strides != 1, Multi-channel -----------------------------------------------------------------
		// X={5,5,25}, W={2,5,9}, B={}, Y={5,2,9}
		{FNeuralTensor(InInputTensorData.FindChecked(90), TArray<int64>({5,2,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,9})),
			FNeuralTensor(ENeuralDataType::Float, {5,5,25}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2}))},
		// X={5,5,5,5}, W={2,5,3,3}, B={}, Y={5,2,2,2}
		{FNeuralTensor(InInputTensorData.FindChecked(40), TArray<int64>({5,2,2,2})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {5,5,5,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2}))},
		// X={5,5,5,5,1}, W={2,5,3,3,1}, B={}, Y={5,2,2,2,1}
		{FNeuralTensor(InInputTensorData.FindChecked(40), TArray<int64>({5,2,2,2,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,3,3,1})),
			FNeuralTensor(ENeuralDataType::Float, {5,5,5,5,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2,1}))},
		// X={5,5,5,5,1,1,1}, W={2,5,3,3,1,1,1}, B={}, Y={5,2,2,2,1,1,1}
		{FNeuralTensor(InInputTensorData.FindChecked(40), TArray<int64>({5,2,2,2,1,1,1})), FNeuralTensor(InInputTensorDataSmall.FindChecked(90), TArray<int64>({2,5,3,3,1,1,1})),
			FNeuralTensor(ENeuralDataType::Float, {5,5,5,5,1,1,1}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(), /*Strides*/FNeuralTensor(TArray<float>({2,2,1,1,1}))},
		// ----------------------------------------------------------------- Manual padding or AutoPad == SameLower/Upper, Dilations != 1, Strides != 1 -----------------------------------------------------------------
		// X={1,1,18}, W={1,1,4}, B={}, Y={1,1,9}, AutoPad = EAutoPad::SameLower || EAutoPad::SameUpper
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,18}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(TArray<float>({2})), /*Strides*/FNeuralTensor(TArray<float>({2})), /*OutputPadding*/FNeuralTensor(), /*OutputShape*/FNeuralTensor(TArray<float>({ 18 }))},
		//										   AutoPad = EAutoPad::SameUpper || EAutoPad::SameLower
		{FNeuralTensor(InInputTensorData.FindChecked(9), TArray<int64>({1,1,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(4), TArray<int64>({1,1,4})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,18}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(),
			/*Dilations*/FNeuralTensor(TArray<float>({2})), /*Strides*/FNeuralTensor(TArray<float>({2})), /*OutputPadding*/FNeuralTensor(), /*OutputShape*/FNeuralTensor(TArray<float>({ 18 }))},
		// X={1,1,100}, W={1,1,3}, B={}, Y={1,1,21}
		{FNeuralTensor(InInputTensorData.FindChecked(21), TArray<int64>({1,1,21})), FNeuralTensor(InInputTensorDataSmall.FindChecked(3), TArray<int64>({1,1,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,100}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({10,5})),
			/*Dilations*/FNeuralTensor(TArray<float>({5})), /*Strides*/FNeuralTensor(TArray<float>({5})), /*OutputPadding*/FNeuralTensor(TArray<float>({ 4 }))},
		// X={1,1,9,9}+{2+1,0+2}, W={1,1,3,3}, B={}, Y={1,1,8,9}
		{FNeuralTensor(InInputTensorData.FindChecked(72), TArray<int64>({1,1,8,9})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({2,1,0,2})),
			/*Dilations*/FNeuralTensor(TArray<float>({2,1})), /*Strides*/FNeuralTensor(TArray<float>({1,1}))},
		// X={1,1,9,9}+{2+1,0+2}, W={1,1,3,3}, B={}, Y={1,1,4,5}
		{FNeuralTensor(InInputTensorData.FindChecked(20), TArray<int64>({1,1,4,5})), FNeuralTensor(InInputTensorDataSmall.FindChecked(9), TArray<int64>({1,1,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,9,9}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({2,1,0,2})),
			/*Dilations*/FNeuralTensor(TArray<float>({2,1})), /*Strides*/FNeuralTensor(TArray<float>({2,2})), /*OutputPadding*/FNeuralTensor(TArray<float>({ 1,0 }))},
		// X={1,1,5,4,5}, W={1,1,3,3,3}, B={}, Y={1,1,3,2,2}
		{FNeuralTensor(InInputTensorData.FindChecked(12), TArray<int64>({1,1,3,2,2})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({3,3,2,3,3,2})),
			/*Dilations*/FNeuralTensor(TArray<float>({3,3,3})), /*Strides*/FNeuralTensor(TArray<float>({2,2,2})), /*OutputPadding*/FNeuralTensor(TArray<float>({ 0,0,1 }))},
		// X={1,1,1,1,5,4,5}, W={1,1,1,1,3,3,3}, B={}, Y={1,1,1,1,3,2,2}
		{FNeuralTensor(InInputTensorData.FindChecked(12), TArray<int64>({1,1,1,1,3,2,2})), FNeuralTensor(InInputTensorDataSmall.FindChecked(27), TArray<int64>({1,1,1,1,3,3,3})),
			FNeuralTensor(ENeuralDataType::Float, {1,1,1,1,5,4,5}), FNeuralTensor(), /*Group*/FNeuralTensor(), /*AutoPad*/FNeuralTensor(), /*Pads*/FNeuralTensor(TArray<float>({0,0,0,0,3,3,2,3,3,2})),
			/*Dilations*/FNeuralTensor(TArray<float>({1,1,3,3,3})), /*Strides*/FNeuralTensor(TArray<float>({1,1,2,2,2})), /*OutputPadding*/FNeuralTensor(TArray<float>({0,0,0,0,1}))}
		});
	TArray<TArray<FNeuralTensor*>> InputTensors;
	TArray<TArray<FNeuralTensor*>> OutputTensors;
	TArray<TSharedPtr<FNeuralOperator>> Operators;
	for (int32 TensorIndex = 0; TensorIndex < Tensors.Num(); ++TensorIndex)
	{
		TArray<FNeuralTensor>& TensorsIndex = Tensors[TensorIndex];
		if (TensorsIndex.Num() > 3 && TensorsIndex[3].Num() > 0)
		{
			InputTensors.Push({ &TensorsIndex[0], &TensorsIndex[1], &TensorsIndex[3] });
		}
		else
		{
			InputTensors.Push({ &TensorsIndex[0], &TensorsIndex[1] });
		}
		OutputTensors.Push({ &TensorsIndex[2] });
		// Set Group - Hacky but simple way of getting group value
		//const int64 Group = (TensorsIndex.Num() > 4 && TensorsIndex[4].Num() > 0 ? FMath::RoundToInt(TensorsIndex[4].At<float>(0)) : 1);
		const TArray<int64> GroupArray = FloatTensorToIntArray(TensorsIndex, 4);
		if (GroupArray.Num() > 1)
		{
			ensureMsgf(false, TEXT("GroupArray.Num() = %d == 0 | 1 failed."), GroupArray.Num());
			return;
		}
		const int64 Group = (GroupArray.Num() > 0 ? GroupArray[0] : 1);
		// Set FConvBaseOperator::EAutoPad - Hacky but simple way of getting this TArray
		FConvBaseOperator::EAutoPad AutoPad = FConvBaseOperator::EAutoPad::NotSet;
		const TArray<int64> AutoPadArray = FloatTensorToIntArray(TensorsIndex, 5);
		if (AutoPadArray.Num() > 0)
		{
			if (AutoPadArray.Num() != 1)
			{
				ensureMsgf(false, TEXT("AutoPadArray should always be 1."));
				return;
			}
			AutoPad = FConvBaseOperator::EAutoPad(AutoPadArray[0]);
		}
		// Set Pads - Hacky but simple way of getting this TArray
		const TArray<int64> Pads = FloatTensorToIntArray(TensorsIndex, 6);
		// Set Dilations - Hacky but simple way of getting this TArray
		const TArray<int64> Dilations = FloatTensorToIntArray(TensorsIndex, 7);
		// Set Strides - Hacky but simple way of getting this TArray
		const TArray<int64> Strides = FloatTensorToIntArray(TensorsIndex, 8);
		// Set OutputPadding - Hacky but simple way of getting this TArray
		const TArray<int64> OutputPadding = FloatTensorToIntArray(TensorsIndex, 9);
		// Set OutputShape - Hacky but simple way of getting this TArray
		const TArray<int64> OutputShape = FloatTensorToIntArray(TensorsIndex, 10);
		// Create and push FConvTransposeOperator
		{
			TSharedPtr<FConvTransposeOperator> ConvTransposeOperator;
			ConvTransposeOperator = MakeShared<FConvTransposeOperator>(AutoPad, Dilations, Group, /*KernelShape*/TArray<int64>(), OutputPadding,
				OutputShape, Pads, Strides);
			// Flip weights
			{
				UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("Flipping FConvTranspose weights. This is a temporary measurement but it will keep"
					" the weights flipped for now (including in the unit test txt files)."));
				ConvTransposeOperator->SetWhetherWeightTensorWasFlipped(true);
				TensorsIndex[1].Flip(2, TensorsIndex[1].GetNumberDimensions());
			}
			Operators.Emplace(ConvTransposeOperator);
		}
		// Remove all non-required/auxiliary tensors from InputTensors to keep it in X, W, and B (if B exists). Otherwise, those would be used by the
		// network and moved into GPU memory
		while (Tensors[TensorIndex].Num() > 4 || Tensors[TensorIndex].Last().Num() < 1)
		{
			Tensors[TensorIndex].Pop();
		}
		// TensorsIndex is no longer valid after this while loop!
	}
	TestOperator(InOutNetworkInferenceQAAsset, Tensors, InputTensors, OutputTensors, Operators, TEXT("FConvTransposeOperator"),
		InGroundTruthDirectory, InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false, /*bInShouldRunGPU*/true);
}

void FOperatorTester::TestReshapeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset,
	const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataPos, const float InZeroThreshold,
	const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FReshapeOperator"), InOperatorCounter, InOperatorTotal);
	// Inout tensors (X != Mean to avoid Output = Bias; Variance positive to avoid NaN)
	TArray<TArray<FNeuralTensor>> Tensors({
		// ----------------------------------------------------------------- ONNX -----------------------------------------------------------------
		// https://github.com/onnx/onnx/blob/master/docs/TestCoverage.md#Reshape
		// reshape
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({4, 2, 3})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({2, 4, 3}))},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({2, 12})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({2, 3, 2, 2}))},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({24})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({2, -1, 2}))},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({-1, 2, 3, 4})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({2, 0, 4, 1}))},
		{FNeuralTensor(InInputTensorData.FindChecked(24), TArray<int64>({2, 3, 4})), FNeuralTensor(TArray<int64>({2, 0, 1, -1})),
			/*Output*/FNeuralTensor()},
		// allowzero
		{FNeuralTensor(FNeuralTensor(ENeuralDataType::Float, TArray<int64>({0, 3, 4}))), FNeuralTensor(TArray<int64>({3, 4, 0})),
			/*Output*/FNeuralTensor(), /*AllowZero=true*/FNeuralTensor()},
		// Scalars
		{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(TArray<int64>({1})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(TArray<int64>({1}))},
		// 1-D
		{FNeuralTensor(InInputTensorData.FindChecked(10)), FNeuralTensor(TArray<int64>({5, 2})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(10)), FNeuralTensor(TArray<int64>({5, -1}))},
		// 2-D
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({3, 6})), FNeuralTensor(TArray<int64>({-1, 3})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(18), TArray<int64>({3, 6})), FNeuralTensor(TArray<int64>({6, 3}))},
		// 3-D
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({3, 5, 2})), FNeuralTensor(TArray<int64>({30})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({3, 5, 2})), FNeuralTensor(TArray<int64>({-1}))},
		// 4-D
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({3, 10, 5, 2})), FNeuralTensor(TArray<int64>({-1, 10, 15})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({3, 10, 5, 2})), FNeuralTensor(TArray<int64>({2, 0, 15}))},
		// 5-D
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({2, 3, 5, 5, 2})), FNeuralTensor(TArray<int64>({2, 10, 15})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(300), TArray<int64>({2, 3, 5, 5, 2})), FNeuralTensor(TArray<int64>({0, -1, 15}))}
		});
	TArray<TArray<FNeuralTensor*>> InputTensors;
	TArray<TArray<FNeuralTensor*>> OutputTensors;
	TArray<TSharedPtr<FNeuralOperator>> Operators;
	for (int32 TensorIndex = 0; TensorIndex < Tensors.Num(); ++TensorIndex)
	{
		InputTensors.Push({ &Tensors[TensorIndex][0], &Tensors[TensorIndex][1] });
		// Not inlined
		if (Tensors[TensorIndex].Num() > 2)
		{
			OutputTensors.Push({ &Tensors[TensorIndex][2] });
			Operators.Push(MakeShared<FReshapeOperator>(/*bIsInlinedTensor*/false, /*AllowZero*/(Tensors[TensorIndex].Num() > 3)));
		}
		// Inlined
		else
		{
			OutputTensors.Push({});
			Operators.Push(MakeShared<FReshapeOperator>(/*bIsInlinedTensor*/true, /*AllowZero*/0));
		}
	}
	TestOperator(InOutNetworkInferenceQAAsset, Tensors, InputTensors, OutputTensors, Operators, TEXT("FReshapeOperator"), InGroundTruthDirectory,
		InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false, /*bInShouldRunGPU*/true);
}

void FOperatorTester::TestSqueezeOperator(UNeuralNetworkInferenceQAAsset* InOutNetworkInferenceQAAsset,
	const TMap<int32, TArray<float>>& InInputTensorData, const TMap<int32, TArray<float>>& InInputTensorDataPos, const float InZeroThreshold,
	const FString& InGroundTruthDirectory, const int32 InOperatorCounter, const int32 InOperatorTotal)
{
	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FSqueezeOperator"), InOperatorCounter, InOperatorTotal);
	// Inout tensors (X != Mean to avoid Output = Bias; Variance positive to avoid NaN)
	TArray<TArray<FNeuralTensor>> Tensors({
		// ----------------------------------------------------------------- ONNX -----------------------------------------------------------------
		// https://github.com/onnx/onnx/blob/master/docs/Operators.md#Squeeze
		// squeeze
		{FNeuralTensor(InInputTensorData.FindChecked(60), TArray<int64>({1, 3, 4, 5})), FNeuralTensor(TArray<int64>({0})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(60), TArray<int64>({1, 3, 4, 5})), FNeuralTensor(TArray<int64>({0}))},
		// squeeze_negative_axis
		{FNeuralTensor(InInputTensorData.FindChecked(15), TArray<int64>({1, 3, 1, 5})), FNeuralTensor(TArray<int64>({-2})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(15), TArray<int64>({1, 3, 1, 5})), FNeuralTensor(TArray<int64>({-2}))},
		// 1-D
		{FNeuralTensor(InInputTensorData.FindChecked(5)), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(), /*Output*/FNeuralTensor()},
		// 2-D
		{FNeuralTensor(InInputTensorData.FindChecked(60), TArray<int64>({12, 5})), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(6), TArray<int64>({6, 1})), FNeuralTensor(TArray<int64>({1})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(6), TArray<int64>({6, 1})), FNeuralTensor(TArray<int64>({1}))},
		{FNeuralTensor(InInputTensorData.FindChecked(6), TArray<int64>({6, 1}))},
		{FNeuralTensor(InInputTensorData.FindChecked(6), TArray<int64>({6, 1})), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(),
			/*Output*/FNeuralTensor()},
		// 3-D
		{FNeuralTensor(InInputTensorData.FindChecked(60), TArray<int64>({3, 4, 5})), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({5, 1, 6})), FNeuralTensor(TArray<int64>({1}))},
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({5, 1, 6})), FNeuralTensor(TArray<int64>({-2})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({5, 1, 6})), FNeuralTensor(TArray<int64>({1})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({5, 1, 6})), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(30), TArray<int64>({5, 1, 6}))},
		// 4-D
		{FNeuralTensor(InInputTensorData.FindChecked(60), TArray<int64>({3, 2, 2, 5})), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(16), TArray<int64>({2, 8, 1, 1})), FNeuralTensor(TArray<int64>({-1, 2})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(16), TArray<int64>({2, 8, 1, 1})), /*Empty*/FNeuralTensor(), /*Empty*/FNeuralTensor(),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(16), TArray<int64>({2, 8, 1, 1}))},
		{FNeuralTensor(InInputTensorData.FindChecked(16), TArray<int64>({2, 8, 1, 1})), FNeuralTensor(TArray<int64>({2})),
			/*Output*/FNeuralTensor()},
		// 5-D
		{FNeuralTensor(InInputTensorData.FindChecked(50), TArray<int64>({1, 5, 1, 5, 2})), FNeuralTensor(TArray<int64>({0, 2})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(50), TArray<int64>({1, 5, 1, 5, 2})), FNeuralTensor(TArray<int64>({-3, -5})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(27), TArray<int64>({3, 1, 3, 1, 3})), FNeuralTensor(TArray<int64>({1, 3})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(27), TArray<int64>({3, 1, 3, 1, 3})), FNeuralTensor(TArray<int64>({3, 1})),
			/*Output*/FNeuralTensor()},
		{FNeuralTensor(InInputTensorData.FindChecked(50), TArray<int64>({1, 5, 1, 5, 2}))}
		//// Other tests (resulting in Warnings and/or unexpected behavior)
		//// Scalars (undefined behavior: 0-d shape array for CPU and crash for GPU)
		//{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(TArray<int64>({0})), /*Output*/FNeuralTensor()},
		//{FNeuralTensor(InInputTensorData.FindChecked(1)), FNeuralTensor(TArray<int64>({0}))},
		//{FNeuralTensor(InInputTensorData.FindChecked(1)), /*Output*/FNeuralTensor()},
		//{FNeuralTensor(InInputTensorData.FindChecked(1))},
		//// Duplicated axis (generating warnings)
		//{FNeuralTensor(InInputTensorData.FindChecked(16), TArray<int64>({2, 8, 1, 1})), FNeuralTensor(TArray<int64>({-2, 2})),
		//	/*Output*/FNeuralTensor()},
		//{FNeuralTensor(InInputTensorData.FindChecked(27), TArray<int64>({3, 1, 3, 1, 3})), FNeuralTensor(TArray<int64>({1, 1})),
		//	/*Output*/FNeuralTensor()},
		//{FNeuralTensor(InInputTensorData.FindChecked(27), TArray<int64>({3, 1, 3, 1, 3})), FNeuralTensor(TArray<int64>({-4, 1})),
		//	/*Output*/FNeuralTensor()},
		//{FNeuralTensor(InInputTensorData.FindChecked(27), TArray<int64>({3, 1, 3, 1, 3})), FNeuralTensor(TArray<int64>({1, -4})),
		//	/*Output*/FNeuralTensor()}
		});
	TArray<TArray<FNeuralTensor*>> InputTensors;
	TArray<TArray<FNeuralTensor*>> OutputTensors;
	TArray<TSharedPtr<FNeuralOperator>> Operators;
	for (int32 TensorIndex = 0; TensorIndex < Tensors.Num(); ++TensorIndex)
	{

		// Two Input Tensors
		if (Tensors[TensorIndex].Num() == 2 || Tensors[TensorIndex].Num() == 3)
		{
			InputTensors.Push({ &Tensors[TensorIndex][0], &Tensors[TensorIndex][1] });
		}
		// One Input Tensor
		else
		{
			InputTensors.Push({ &Tensors[TensorIndex][0] });
		}
		
		// Not inlined
		if (Tensors[TensorIndex].Num() == 3)
		{
			OutputTensors.Push({ &Tensors[TensorIndex][2] });
			Operators.Push(MakeShared<FSqueezeOperator>(/*bIsInlinedTensor*/false));
		}
		else if (Tensors[TensorIndex].Num() == 4)
		{
			OutputTensors.Push({ &Tensors[TensorIndex][3] });
			Operators.Push(MakeShared<FSqueezeOperator>(/*bIsInlinedTensor*/false));
		}
		// Inlined
		else
		{
			OutputTensors.Push({});
			Operators.Push(MakeShared<FSqueezeOperator>(/*bIsInlinedTensor*/true));
		}
	}
	TestOperator(InOutNetworkInferenceQAAsset, Tensors, InputTensors, OutputTensors, Operators, TEXT("FSqueezeOperator"), InGroundTruthDirectory,
		InZeroThreshold, /*bShouldOperatorsProvideSameResult*/false, /*bInShouldRunGPU*/true);
}

/* FOperatorTester static private functions
 *****************************************************************************/

TArray<int64> FOperatorTester::FloatTensorToIntArray(const TArray<FNeuralTensor>& InTensors, const int32 InTensorIndex)
{
	// Set OutputArray - Hacky but simple way of getting dilation values
	TArray<int64> OutputArray;
	if (InTensors.Num() > InTensorIndex && InTensors[InTensorIndex].Num() > 0)
	{
		const FNeuralTensor& Tensor = InTensors[InTensorIndex];
		OutputArray.SetNumUninitialized(Tensor.Num());
		for (int32 Index = 0; Index < Tensor.Num(); ++Index)
		{
			OutputArray[Index] = FMath::RoundToInt(Tensor.At<float>(Index));
		}
	}
	return OutputArray;
}

void FOperatorTester::ResetOrCheckInput(UNeuralNetwork* InOutNetwork, const TArray<FNeuralTensor>& InInputTensorArray, const bool bIsInlinedTensor)
{
	if (bIsInlinedTensor)
	{
		FNeuralNetworkInferenceQAUtils::SetInputFromArrayCopy(InOutNetwork, InInputTensorArray);
	}
	// Check input has not changed
	else
	{
		for (int32 InputIndex = 0; InputIndex < InOutNetwork->GetInputTensorNumber(); ++InputIndex)
		{
			const FNeuralTensor& InputTensor = InOutNetwork->GetInputTensor(InputIndex);
			ensureMsgf(InInputTensorArray[InputIndex].Num() == InputTensor.Num(),
				TEXT("InInputTensorArray[InputIndex].Num() == InputTensor.Num() failed (%d != %d)."),
				InInputTensorArray[InputIndex].Num(), InputTensor.Num());
			ensureMsgf(FNeuralNetworkInferenceQAUtils::EstimateTensorL1DiffError(InputTensor, InInputTensorArray[InputIndex], 0.f,
				TEXT("ResetOrCheckInput")), TEXT("ResetOrCheckInput() did not pass."));
		}
	}
}

void FOperatorTester::NetworkGPUvsCPU(UNeuralNetwork* InOutNetwork, const bool bIsInlinedTensor, const float InZeroThreshold)
{
	// Create InputTensorDataArray
	const TArray<FNeuralTensor> InputTensorArray = FNeuralNetworkInferenceQAUtils::CreateInputArrayCopy(InOutNetwork);
	// GPU
	InOutNetwork->SetDeviceType(ENeuralDeviceType::GPU);
	InOutNetwork->Run();
	// Store GPU results
	const TArray<FNeuralTensor> OutputTensorArray = FNeuralNetworkInferenceQAUtils::CreateOutputArrayCopy(InOutNetwork);
	// Reset memory / Check input has not changed
	ResetOrCheckInput(InOutNetwork, InputTensorArray, bIsInlinedTensor);
	// CPU
	InOutNetwork->SetDeviceType(ENeuralDeviceType::CPU);
	InOutNetwork->Run();
	// CPU vs. GPU
	for (int32 OutputIndex = 0; OutputIndex < InOutNetwork->GetOutputTensorNumber(); ++OutputIndex)
	{
		const FNeuralTensor& OutputTensorCPU = OutputTensorArray[OutputIndex];
		const FNeuralTensor& OutputTensorGPU = InOutNetwork->GetOutputTensor(OutputIndex);
		ensureMsgf(OutputTensorCPU.Num() == OutputTensorGPU.Num(), TEXT("OutputTensorCPU.Num() == OutputTensorGPU.Num(): %d vs. %d"),
			OutputTensorCPU.Num(), OutputTensorGPU.Num());
		ensureMsgf(FNeuralNetworkInferenceQAUtils::EstimateTensorL1DiffError(OutputTensorCPU, OutputTensorGPU, InZeroThreshold, TEXT("GPUvsCPU")),
			TEXT("CPU vs. GPU error has too high"));
	}
	// Reset memory / Check input has not changed
	ResetOrCheckInput(InOutNetwork, InputTensorArray, bIsInlinedTensor); // This is doing an unnecessary copy (it should move really)
}

void FOperatorTester::NetworkInputVsOutput(UNeuralNetwork* InOutNetwork, const int32 InInlinedTensorIndex)
{
	const TArray<FNeuralTensor> InputTensorArray = FNeuralNetworkInferenceQAUtils::CreateInputArrayCopy(InOutNetwork);
	// Run UNeuralNetwork
	InOutNetwork->Run();
	// Check input equals one of the outputs
	const FNeuralTensor& InputTensor = InOutNetwork->GetInputTensor(InInlinedTensorIndex);
	bool bIsSomeInputAnOutput = false;
	for (int32 OutputIndex = 0; OutputIndex < InOutNetwork->GetOutputTensorNumber(); ++OutputIndex)
	{
		const FNeuralTensor& OutputTensor = InOutNetwork->GetOutputTensor(OutputIndex);
		if (InputTensor.GetUnderlyingUInt8ArrayRef() == OutputTensor.GetUnderlyingUInt8ArrayRef())
		{
			bIsSomeInputAnOutput = true;
			break;
		}
	}
	ensureMsgf(bIsSomeInputAnOutput, TEXT("Input is not being used as an output."));
	// Reset memory
	FNeuralNetworkInferenceQAUtils::SetInputFromArrayCopy(InOutNetwork, InputTensorArray); // This is doing an unnecessary copy (it should move)
}
