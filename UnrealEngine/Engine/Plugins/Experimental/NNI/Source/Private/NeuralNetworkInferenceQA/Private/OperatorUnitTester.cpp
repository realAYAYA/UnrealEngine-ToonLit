// Copyright Epic Games, Inc. All Rights Reserved.

#include "OperatorUnitTester.h"
#include "NeuralEnumClasses.h"
#include "NeuralNetworkInferenceQAUtils.h"
#include "NeuralOperator.h"
#include "NeuralOperators.h"
#include "NeuralTensor.h"
#include "OperatorTester.h"
#include "Math/RandomStream.h"



/* FOperatorUnitTester static public functions
 *****************************************************************************/

bool FOperatorUnitTester::GlobalTest(const FString& InProjectContentDir, const FString& InUnitTestRelativeDirectory)
{
	const FString UnitTestDirectory = InProjectContentDir / InUnitTestRelativeDirectory;
	const float ZeroThreshold = 0.f;
	const float FloatThreshold = 5e-7;
	const float RelaxedFloatThreshold = 5e-6;
	const float VeryRelaxedFloatThreshold = 4e-5;
	const float VeryVeryRelaxedFloatThreshold = 2e-4;
	const float SuperRelaxedFloatThreshold = 1.5e-3;
	// Input arrays
	const TArray<int32> ArraySizesToPopulate({
		1,
		2,
		3,
		4, // 2^2
		5,
		6, // 2x3
		8, // 2^3
		9, // 3^2
		10, // 2x5
		12, // 2^2 x 3
		15, // 3x5
		16, // 2^4
		18, // 2x3x3
		20, // 2^2 x 5
		21, // 3x7
		23,
		24, // 2^3 x 3
		25, // 5^2
		27, // 3^3
		30, // 2 x 3 x 5
		32, // 2^5
		36, // 2^2 x 3^2
		40, // 2^3 x 5
		46, // 2 x 23
		50, // 2 x 5^2
		60, // 1 x 2^2 x 3 x 5
		64, // 2^6
		69, // 3 x 23
		72, // 2^3 x 3^2
		80, // 2^4 x 5
		81, // 3^4
		90, // 2 x 3^2 x 5
		100, // 2^2 x 5^2
		108, // 2^2 x 3^3
		112, // 2^4 x 7
		120, // 2^3 x 3 x 5
		125, // 5^3
		128, // 2^7
		150, // 2 x 3 x 5^2
		160, // 2^5 x 5
		162, // 2 x 3^4
		240, // 2^4 x 3 x 5
		243, // 3^5
		256, // 2^8
		300, // 2^2 x 3 x 5^2
		486, // 2 x 3^5
		512, // 2^9
		625, // 5^4
		810, // 2 x 3^4 x 5
		1800, // 2^3 x 3^2 x 5^21
		2700 // 2^2 x 3^3 x 5^2
		// To be sorted out properly later
		// [Add new numbers here, DO NOT add above this line]
	});
	TMap<int32, TArray<float>> InputTensorData;
	TMap<int32, TArray<float>> InputTensorDataSmall;
	TMap<int32, TArray<float>> InputTensorDataTiny;
	TMap<int32, TArray<float>> InputTensorDataPos;
	CreateRandomArraysOfFixedSeedAndDefinedSizes(InputTensorData, InputTensorDataSmall, InputTensorDataTiny, InputTensorDataPos, ArraySizesToPopulate);
	// Input tensors
	const TArray<int64> Sizes4D({ 2, 3, 5, 5 });
	const FNeuralTensor InputTensor0D(InputTensorData.FindChecked(1));
	const FNeuralTensor InputTensor1D_150(InputTensorData.FindChecked(150));
	const FNeuralTensor InputTensor2D_18(InputTensorData.FindChecked(18), TArray<int64>({ 1, 18 }));
	const FNeuralTensor InputTensor3D_300(InputTensorData.FindChecked(300), TArray<int64>({ 5, 6, 10 }));
	const FNeuralTensor InputTensor4D_150(InputTensorData.FindChecked(150), TArray<int64>({ 2, 3, 5, 5 }));
	const FNeuralTensor InputTensorSmall0D(InputTensorDataSmall.FindChecked(1));
	const FNeuralTensor InputTensorSmall1D_150(InputTensorDataSmall.FindChecked(150));
	const FNeuralTensor InputTensorSmall2D_18(InputTensorDataSmall.FindChecked(18), InputTensor2D_18.GetSizes());
	const FNeuralTensor InputTensorSmall3D_300(InputTensorDataSmall.FindChecked(300), InputTensor3D_300.GetSizes());
	const FNeuralTensor InputTensorSmall4D_150(InputTensorDataSmall.FindChecked(150), InputTensor4D_150.GetSizes());
	const FNeuralTensor InputTensorTiny0D(InputTensorDataTiny.FindChecked(1), InputTensor0D.GetSizes());
	const FNeuralTensor InputTensorTiny1D_150(InputTensorDataTiny.FindChecked(150));
	const FNeuralTensor InputTensorTiny2D_18(InputTensorDataTiny.FindChecked(18), InputTensor2D_18.GetSizes());
	const FNeuralTensor InputTensorTiny3D_300(InputTensorDataTiny.FindChecked(300), InputTensor3D_300.GetSizes());
	const FNeuralTensor InputTensorTiny4D_150(InputTensorDataTiny.FindChecked(150), InputTensor4D_150.GetSizes());
	// Inout tensors for Elementwise Operators
	const TArray<TArray<FNeuralTensor>> ElementWiseTensors({ {InputTensor0D}, {InputTensor1D_150, InputTensor1D_150}, {InputTensor2D_18},
		{InputTensor3D_300, InputTensor3D_300}, {InputTensor4D_150} });
	const TArray<TArray<FNeuralTensor>> ElementWiseTensorsSmall({ {InputTensorSmall0D}, {InputTensorSmall1D_150, InputTensorSmall1D_150},
		{InputTensorSmall2D_18}, {InputTensorSmall3D_300, InputTensorSmall3D_300}, {InputTensorSmall4D_150} });
	const TArray<TArray<FNeuralTensor>> ElementWiseTensorsTiny({ {InputTensorTiny0D}, {InputTensorTiny1D_150, InputTensorTiny1D_150},
		{InputTensorTiny2D_18}, {InputTensorTiny3D_300, InputTensorTiny3D_300}, {InputTensorTiny4D_150} });

	// Inout tensors for Multidirectional Broadcasting Operators
	const TArray<int64> Sizes7D({ 2, 3, 3, 2, 5, 3, 5 });
	const TArray<TArray<FNeuralTensor>> MultidirectionalTensors({
		// Scalars
		{InputTensor0D, InputTensorSmall0D, FNeuralTensor(ENeuralDataType::Float, InputTensor0D.GetSizes())},
		// 1-D, 2-D, 4-D, 7-D
		{FNeuralTensor(InputTensorData.FindChecked(150)), FNeuralTensor(InputTensorDataSmall.FindChecked(150)), FNeuralTensor(ENeuralDataType::Float, 150)},
		{InputTensor2D_18, InputTensorSmall2D_18, FNeuralTensor(ENeuralDataType::Float, InputTensor2D_18.GetSizes())},
		{InputTensor4D_150, InputTensorSmall4D_150, FNeuralTensor(ENeuralDataType::Float, InputTensor4D_150.GetSizes())},
		{FNeuralTensor(InputTensorData.FindChecked(2700), Sizes7D), FNeuralTensor(InputTensorDataSmall.FindChecked(2700), Sizes7D),
			FNeuralTensor(ENeuralDataType::Float, Sizes7D)},
		// Multidirectional Broadcasting tests - One of the inputs is 1D
		{InputTensor4D_150, InputTensorSmall0D, FNeuralTensor(ENeuralDataType::Float, InputTensor4D_150.GetSizes())},
		{InputTensor0D, InputTensorSmall2D_18, FNeuralTensor(ENeuralDataType::Float, InputTensor2D_18.GetSizes())},
		// Multidirectional Broadcasting tests - Random input sizes
		{FNeuralTensor(InputTensorData.FindChecked(3)), FNeuralTensor(InputTensorDataSmall.FindChecked(3), TArray<int64>({ 3, 1 })),
			FNeuralTensor(ENeuralDataType::Float, TArray<int64>{3, 3})}, // [3].^[3, 1] --> [3,3]
		{FNeuralTensor(InputTensorData.FindChecked(3), TArray<int64>({ 3, 1 })), FNeuralTensor(InputTensorDataSmall.FindChecked(3)),
			FNeuralTensor(ENeuralDataType::Float, TArray<int64>{3, 3})}, // [3, 1].^[3] --> [3,3]
		// {2, 1, 2, 1, 3, 5, 5} .^ {2, 2, 5, 3, 1, 5} --> {2, 2, 2, 5, 3, 5, 5}
		{FNeuralTensor(InputTensorData.FindChecked(300), TArray<int64>({ 2, 1, 2, 1, 3, 5, 5 })),
			FNeuralTensor(InputTensorDataSmall.FindChecked(300), TArray<int64>({ 2, 2, 5, 3, 1, 5 })),
			FNeuralTensor(ENeuralDataType::Float, TArray<int64>{2, 2, 2, 5, 3, 5, 5})}
		});
	// Get all operator names
	const TArray<FString> OperatorNames({ TEXT("FAbsOperator"), TEXT("FAcosOperator"), TEXT("FAddOperator"), TEXT("FAsinOperator"), TEXT("FAtanOperator"), TEXT("FBatchNormalizationOperator"), TEXT("FCeilOperator"),
		TEXT("FConvOperator"), TEXT("FConvTransposeOperator"), TEXT("FCosOperator"), TEXT("FCoshOperator"), TEXT("FDivOperator"), TEXT("FExpOperator"), TEXT("FFloorOperator"), TEXT("FGemmOperator"),
		TEXT("FLeakyReluOperator"), TEXT("FLogOperator"), TEXT("FMulOperator"), TEXT("FNegOperator"), TEXT("FPowOperator"), TEXT("FReciprocalOperator"), TEXT("FReluOperator"), TEXT("FReshapeOperator"),
		TEXT("FRoundOperator"), TEXT("FSigmoidOperator"), TEXT("FSignOperator"), TEXT("FSinOperator"), TEXT("FSinhOperator"), TEXT("FSqrtOperator"), TEXT("FSqueezeOperator"), TEXT("FSubOperator"), 
		TEXT("FTanOperator"), TEXT("FTanhOperator") });
	// Creating NeuralNetworkInferenceQAAsset
	UNeuralNetworkInferenceQAAsset* NeuralNetworkInferenceQAAsset = UNeuralNetworkInferenceQAAsset::Load(InUnitTestRelativeDirectory, TEXT("NeuralNetworkInferenceQAAsset"));
	if (!NeuralNetworkInferenceQAAsset)
	{
		ensureMsgf(false, TEXT("NeuralNetworkInferenceQAAsset could not be loaded!"));
		return false;
	}
	NeuralNetworkInferenceQAAsset->FindOrAddOperators(OperatorNames);
	NeuralNetworkInferenceQAAsset->FlushNewTests();
	// Per operator testing (checking false if errors occur)
	const int32 OperatorTotal = OperatorNames.Num(); // 164;
	int32 OperatorCounter = 0;
	FOperatorTester::TestElementWiseOperator<FAbsOperator>(NeuralNetworkInferenceQAAsset, TEXT("FAbsOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FAcosOperator>(NeuralNetworkInferenceQAAsset, TEXT("FAcosOperator"), ElementWiseTensorsTiny, VeryVeryRelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestMultiDirectionalBroadcastingOperator<FAddOperator>(NeuralNetworkInferenceQAAsset, TEXT("FAddOperator"), MultidirectionalTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FAsinOperator>(NeuralNetworkInferenceQAAsset, TEXT("FAsinOperator"), ElementWiseTensorsTiny, VeryVeryRelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FAtanOperator>(NeuralNetworkInferenceQAAsset, TEXT("FAtanOperator"), ElementWiseTensors, VeryVeryRelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestBatchNormalizationOperator(NeuralNetworkInferenceQAAsset, InputTensorData, InputTensorDataSmall, InputTensorDataPos, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FCeilOperator>(NeuralNetworkInferenceQAAsset, TEXT("FCeilOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestConvOperator(NeuralNetworkInferenceQAAsset, InputTensorData, InputTensorDataSmall, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestConvTransposeOperator(NeuralNetworkInferenceQAAsset, InputTensorData, InputTensorDataSmall, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FCosOperator>(NeuralNetworkInferenceQAAsset, TEXT("FCosOperator"), ElementWiseTensors, VeryRelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FCoshOperator>(NeuralNetworkInferenceQAAsset, TEXT("FCoshOperator"), ElementWiseTensorsSmall, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestMultiDirectionalBroadcastingOperator<FDivOperator>(NeuralNetworkInferenceQAAsset, TEXT("FDivOperator"), MultidirectionalTensors, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FExpOperator>(NeuralNetworkInferenceQAAsset, TEXT("FExpOperator"), ElementWiseTensorsSmall, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FFloorOperator>(NeuralNetworkInferenceQAAsset, TEXT("FFloorOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	//UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FGather"), ++OperatorCounter, OperatorTotal);
	//{
	//	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- Not implemented (yet)"));
	//}
	FOperatorTester::TestGemmOperator(NeuralNetworkInferenceQAAsset, InputTensorData, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWise1AttributeOperator<FLeakyReluOperator>(NeuralNetworkInferenceQAAsset, TEXT("FLeakyReluOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FLogOperator>(NeuralNetworkInferenceQAAsset, TEXT("FLogOperator"), ElementWiseTensors, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestMultiDirectionalBroadcastingOperator<FMulOperator>(NeuralNetworkInferenceQAAsset, TEXT("FMulOperator"), MultidirectionalTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FNegOperator>(NeuralNetworkInferenceQAAsset, TEXT("FNegOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestMultiDirectionalBroadcastingOperator<FPowOperator>(NeuralNetworkInferenceQAAsset, TEXT("FPowOperator"), MultidirectionalTensors, RelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FReciprocalOperator>(NeuralNetworkInferenceQAAsset, TEXT("FReciprocalOperator"), ElementWiseTensors, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	//UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FReduceSum"), ++OperatorCounter, OperatorTotal);
	//{
	//	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- Not implemented (yet)"));
	//}
	FOperatorTester::TestElementWiseOperator<FReluOperator>(NeuralNetworkInferenceQAAsset, TEXT("FReluOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestReshapeOperator(NeuralNetworkInferenceQAAsset, InputTensorData, InputTensorDataPos, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FRoundOperator>(NeuralNetworkInferenceQAAsset, TEXT("FRoundOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	//UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FShape"), ++OperatorCounter, OperatorTotal);
	//{
	//	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- Not implemented (yet)"));
	//}
	FOperatorTester::TestElementWiseOperator<FSigmoidOperator>(NeuralNetworkInferenceQAAsset, TEXT("FSigmoidOperator"), ElementWiseTensors, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FSignOperator>(NeuralNetworkInferenceQAAsset, TEXT("FSignOperator"), ElementWiseTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FSinOperator>(NeuralNetworkInferenceQAAsset, TEXT("FSinOperator"), ElementWiseTensors, VeryRelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FSinhOperator>(NeuralNetworkInferenceQAAsset, TEXT("FSinhOperator"), ElementWiseTensorsSmall, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FSqrtOperator>(NeuralNetworkInferenceQAAsset, TEXT("FSqrtOperator"), ElementWiseTensors, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestSqueezeOperator(NeuralNetworkInferenceQAAsset, InputTensorData, InputTensorDataPos, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestMultiDirectionalBroadcastingOperator<FSubOperator>(NeuralNetworkInferenceQAAsset, TEXT("FSubOperator"), MultidirectionalTensors, ZeroThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FTanOperator>(NeuralNetworkInferenceQAAsset, TEXT("FTanOperator"), ElementWiseTensors, SuperRelaxedFloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	FOperatorTester::TestElementWiseOperator<FTanhOperator>(NeuralNetworkInferenceQAAsset, TEXT("FTanhOperator"), ElementWiseTensorsSmall, FloatThreshold, UnitTestDirectory, ++OperatorCounter, OperatorTotal);
	//UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("--------------- %d/%d FUnsqueeze"), ++OperatorCounter, OperatorTotal);
	//{
	//	UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("-------------------- Not implemented (yet)"));
	//}
	// Verify operator count
	ensureMsgf(OperatorTotal == OperatorCounter, TEXT("New or missing operators found, OperatorTotal == OperatorCounter failed (%d != %d)"), OperatorTotal, OperatorCounter);

	// Test against GT saved on disk - Only saving a unique UAsset file with all results from all operators
	if (!NeuralNetworkInferenceQAAsset->CompareNewVsPreviousTests(UnitTestDirectory))
	{
		// Saving NeuralNetworkInferenceQAAsset
		ensureMsgf(NeuralNetworkInferenceQAAsset->Save(), TEXT("NeuralNetworkInferenceQAAsset->Save() failed."));
		// Error message
		ensureMsgf(false, TEXT("NeuralNetworkInferenceQAAsset->CompareNewVsPreviousTests() failed. Updated NeuralNetworkInferenceQAAsset saved as %s. Check the previous warning messages."), *NeuralNetworkInferenceQAAsset->GetOutermost()->GetLoadedPath().GetLocalFullPath());
		// Test failed
		return false;
	}
	// Test successful
	return true;
}



/* FOperatorUnitTester static public functions
 *****************************************************************************/

void FOperatorUnitTester::CreateRandomArraysOfFixedSeedAndDefinedSizes(TMap<int32, TArray<float>>& InOutInputTensorData, TMap<int32, TArray<float>>& InOutInputTensorDataSmall,
	TMap<int32, TArray<float>>& InOutInputTensorDataTiny, TMap<int32, TArray<float>>& InOutInputTensorDataPos, const TArray<int32>& InArraySizesToPopulate)
{
	FRandomStream InOutRandomStream(99/*FMath::Rand()*/);
	const float InRange = 1e2;
	const float InRangeSmall = 4;
	const float InRangeTiny = 2;
	for (const int32 ArraySize : InArraySizesToPopulate)
	{
		InOutInputTensorData.Add(ArraySize, CreateRandomArray(&InOutRandomStream, ArraySize, -InRange, InRange));
		InOutInputTensorDataSmall.Add(ArraySize, CreateRandomArray(&InOutRandomStream, ArraySize, -InRangeSmall, InRangeSmall));
		InOutInputTensorDataTiny.Add(ArraySize, CreateRandomArray(&InOutRandomStream, ArraySize, -InRangeTiny, InRangeTiny));
		InOutInputTensorDataPos.Add(ArraySize, CreateRandomArray(&InOutRandomStream, ArraySize, 0, InRange));
	}
}

TArray<float> FOperatorUnitTester::CreateRandomArray(FRandomStream* InOutRandomStream, const int32 InSize, const float InRangeMin, const float InRangeMax)
{
	if (!InOutRandomStream)
	{
		ensureMsgf(false, TEXT("InOutRandomStream was a nullptr."));
		return TArray<float>();
	}
	TArray<float> OutArray;
	OutArray.SetNum(InSize);
	for (float& FloatValue : OutArray)
	{
		FloatValue = InOutRandomStream->FRandRange(InRangeMin, InRangeMax);
	}
	return OutArray;
}
