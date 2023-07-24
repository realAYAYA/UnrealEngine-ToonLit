// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralNetworkInferenceQAUtils.h"
#include "NeuralTimer.h"



/* FNeuralNetworkInferenceQAUtils static public functions
 *****************************************************************************/

bool FNeuralNetworkInferenceQAUtils::EstimateTensorL1DiffError(const FNeuralTensor& InTensorA, const FNeuralTensor& InTensorB,
	const float InZeroThreshold, const FString& InDebugName)
{
	if (InTensorA.GetDataType() != InTensorB.GetDataType())
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("InTensorA.GetDataType() != InTensorB.GetDataType() failed, %d != %d."),
			InTensorA.GetDataType(), InTensorB.GetDataType());
		return false;
	}
	if (InTensorA.Num() != InTensorB.Num())
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("InTensorA.Num() == InTensorB.Num() failed, %d != %d."), InTensorA.Num(),
			InTensorB.Num());
		return false;
	}
	const int32 TensorSize = InTensorA.Num();
	float AverageL1Value = 0.f;
	float AverageL1Diff = 0.f;
	int32 SizeMinusInvalid = TensorSize;
	FString FiniteVsNon;
	// Float
	const ENeuralDataType DataType = InTensorA.GetDataType();
	if (DataType == ENeuralDataType::Float)
	{
		for (int32 Index = 0; Index < TensorSize; ++Index)
		{
			const float Value1 = InTensorA.At<float>(Index);
			const float Value2 = InTensorB.At<float>(Index);
			const bool bIsFinite1 = FMath::IsFinite(Value1);
			const bool bIsFinite2 = FMath::IsFinite(Value2);
			// Update L1
			if (bIsFinite1 && bIsFinite2)
			{
				AverageL1Value += FMath::Abs(Value1);
				AverageL1Diff += FMath::Abs(Value1 - Value2);
			}
			// Update NaN|Inf stats
			else
			{
				if (bIsFinite1 != bIsFinite2)
				{
					FiniteVsNon += (bIsFinite1 ? FString::SanitizeFloat(Value1) : FString(TEXT("NaNInf"))) + TEXT("-")
						+ (bIsFinite2 ? FString::SanitizeFloat(Value2) : FString(TEXT("NaNInf"))) + TEXT("; ");
				}
				--SizeMinusInvalid;
			}
		}
	}
	// Integer types
	else
	{
		for (int32 Index = 0; Index < TensorSize; ++Index)
		{
			float Value1;
			float Value2;
			if (DataType == ENeuralDataType::Int32)
			{
				Value1 = InTensorA.At<int32>(Index);
				Value2 = InTensorB.At<int32>(Index);
			}
			else if (DataType == ENeuralDataType::Int64)
			{
				Value1 = InTensorA.At<int64>(Index);
				Value2 = InTensorB.At<int64>(Index);
			}
			else if (DataType == ENeuralDataType::UInt32)
			{
				Value1 = InTensorA.At<uint32>(Index);
				Value2 = InTensorB.At<uint32>(Index);
			}
			else if (DataType == ENeuralDataType::UInt64)
			{
				Value1 = InTensorA.At<uint64>(Index);
				Value2 = InTensorB.At<uint64>(Index);
			}
			else
			{
				Value1 = 0.f;
				Value2 = 0.f;
				ensureMsgf(false, TEXT("DataType = %d not implemented yet for FNeuralNetworkInferenceQAUtils::EstimateTensorL1DiffError()!"),
					InTensorA.GetDataType());
			}
			// Update L1
			AverageL1Value += FMath::Abs(Value1);
			AverageL1Diff += FMath::Abs(Value1 - Value2);
		}
	}

	// Were all numbers NaN|Inf?
	if (SizeMinusInvalid < 1)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("All %d numbers were NaN|Inf."), TensorSize);
		return true;
	}
	// Some verbose
	if (FiniteVsNon.Len() > 0)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Display, TEXT("IsFinite(Value1) != IsFinite(Value2) for %d values (skipped %d valid ones): %s"),
			TensorSize - SizeMinusInvalid, SizeMinusInvalid, *FiniteVsNon);
	}
	// Average L1
	AverageL1Value /= SizeMinusInvalid;
	AverageL1Diff /= (SizeMinusInvalid * (AverageL1Value > 0.01f ? AverageL1Value : 1)); // Relative if > 0.01, absolute otherwise
	// Sanity check
	if (!FMath::IsFinite(AverageL1Value) || !FMath::IsFinite(AverageL1Diff))
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning,
			TEXT("FNeuralNetworkInferenceQAUtils::EstimateTensorL1DiffError(): NaN|Inf found: AverageL1Diff %f for an L1 average value of %f."),
			AverageL1Diff, AverageL1Value);
		return false;
	}
	// Did it pass?
	if (AverageL1Diff > InZeroThreshold)
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning,
			TEXT("%s: AverageL1Diff <= ZeroThreshold failed (%fe-3 > %fe-3), AverageL1Value = %f, AverageL1Diff/ZeroThreshold = %f."),
			*InDebugName, 1e3 * AverageL1Diff, 1e3 * InZeroThreshold, AverageL1Value, AverageL1Diff / InZeroThreshold);
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("- TensorA = %s"), *InTensorA.ToString(100));
		UE_LOG(LogNeuralNetworkInferenceQA, Warning, TEXT("- TensorB = %s"), *InTensorB.ToString(100));
		return false;
	}
	return true;
}

TArray<FNeuralTensor> FNeuralNetworkInferenceQAUtils::CreateInputArrayCopy(const UNeuralNetwork* const InNeuralNetwork)
{
	TArray<FNeuralTensor> InputTensorArray;
	for (uint32 InputTensorIndex = 0; InputTensorIndex < InNeuralNetwork->GetInputTensorNumber(); ++InputTensorIndex)
	{
		const FNeuralTensor& InputTensor = InNeuralNetwork->GetInputTensor(InputTensorIndex);
		InputTensorArray.Push(InputTensor);
	}
	return InputTensorArray;
}

void FNeuralNetworkInferenceQAUtils::SetInputFromArrayCopy(UNeuralNetwork* InOutNeuralNetwork, const TArray<FNeuralTensor>& InInputTensorArray)
{
	// const FScopeLock ResourcesLock(&ResoucesCriticalSection);
	if (InOutNeuralNetwork->GetInputTensorNumber() != InInputTensorArray.Num())
	{
		UE_LOG(LogNeuralNetworkInferenceQA, Warning,
			TEXT("FNeuralNetworkInferenceQAUtils::SetInputFromArrayCopy(): GetInputTensorNumber() == InInputTensorArray.Num() failed, %d != %d."),
			InOutNeuralNetwork->GetInputTensorNumber(), InInputTensorArray.Num());
		return;
	}
	for (uint32 InputTensorIndex = 0; InputTensorIndex < InOutNeuralNetwork->GetInputTensorNumber(); ++InputTensorIndex)
	{
		InOutNeuralNetwork->SetInputFromVoidPointerCopy(InInputTensorArray[InputTensorIndex].GetUnderlyingUInt8ArrayRef().GetData(), InputTensorIndex);
	}
}

TArray<FNeuralTensor> FNeuralNetworkInferenceQAUtils::CreateOutputArrayCopy(const UNeuralNetwork* const InNeuralNetwork)
{
	TArray<FNeuralTensor> OutputTensorArray;
	for (uint32 OutputTensorIndex = 0; OutputTensorIndex < InNeuralNetwork->GetOutputTensorNumber(); ++OutputTensorIndex)
	{
		const FNeuralTensor& OutputTensor = InNeuralNetwork->GetOutputTensor(OutputTensorIndex);
		OutputTensorArray.Push(OutputTensor);
	}
	return OutputTensorArray;
}
