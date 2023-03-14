// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ReshapeOperator.h"
#include "CopyCS.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FReshapeOperator structors
 *****************************************************************************/

FReshapeOperator::FReshapeOperator(const bool bIsInlinedTensor, const FNodeProto* const InNodeProto)
	: FReshapeOperator(bIsInlinedTensor)
{
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FReshapeOperator(): InNodeProto was a nullptr."));
		return;
	}
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("AllowZero"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		AllowZero = MomentumAttribute->I;
	}
}

FReshapeOperator::FReshapeOperator(const bool bIsInlinedTensor, const int64 InAllowZero)
	: FNeuralOperator(TEXT("Reshape"), 14, (bIsInlinedTensor ? 0 : -1))
	, AllowZero(InAllowZero)
{
}

FReshapeOperator::~FReshapeOperator()
{
}



/* FReshapeOperator public functions
 *****************************************************************************/

bool FReshapeOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Output initialization and size sanity check
	if (InlinedTensor < 0)
	{
		const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
		const FNeuralTensor& InputTensor = *InputTensors[0];
		const FNeuralTensor& ShapeTensor = *InputTensors[1];
		FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
		// Initialize OutputTensor
		const TArray<int64> OutputShape = FReshapeOperator::Remove0sAndMinus1sFromShape(ShapeTensor, InputTensor);
		OutputTensor.SetNumUninitialized(InputTensor.GetDataType(), OutputShape);
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

bool FReshapeOperator::SetWhetherInputTensorsNeedTransferToGPUForGPUMode()
{
	// Enable GPU mode only on the first input tensor
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	InputTensors[0]->SetEnableGPU(true);
	return true;
}

void FReshapeOperator::ForwardCPU()
{
	// Sanity check
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FReshapeOperator::ForwardCPU(): bIsLoaded was false."));
		return;
	}
	// Reshape is 0-cost if inlined, otherwise, just copy memory from input to output
	const TArray<FNeuralTensor*> InputTensors = GetInputTensorsConst();
	const FNeuralTensor& InputTensor = *InputTensors[0];
	const FNeuralTensor& ShapeTensor = *InputTensors[1];
	FNeuralTensor& OutputTensor = (InlinedTensor < 0 ? GetOutputTensorNoConst() : *GetInputTensorsNoConst()[0]);
	// Update InputSizeIfInlined (if inlined)
	if (InlinedTensor > -1)
	{
		InputSizeIfInlined = InputTensor.GetSizes();
	}
	// Copy memory (if not inlined)
	else
	{
		OutputTensor.SetFromUnderlyingUInt8ArrayCopy(InputTensor.GetUnderlyingUInt8ArrayRef());
	}
	// Reshape output
	TArray<int64> OutputShape = FReshapeOperator::Remove0sAndMinus1sFromShape(ShapeTensor, InputTensor);
	OutputTensor.ReshapeMove(OutputShape);
}

void FReshapeOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FReshapeOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
		return;
	}
	// Reshape is 0-cost if inlined, otherwise, just copy memory from input to output
	const TArray<FNeuralTensor*> InputTensors = GetInputTensorsConst();
	const FNeuralTensor& InputTensor = *InputTensors[0];
	const FNeuralTensor& ShapeTensor = *InputTensors[1];
	FNeuralTensor& OutputTensor = (InlinedTensor < 0 ? GetOutputTensorNoConst() : *GetInputTensorsNoConst()[0]);
	// If input is empty, it means it has a GetSizes() similar to [a, b, 0, c, ...]
	if (InputTensor.IsEmpty())
	{
		return;
	}
	// Update InputSizeIfInlined (if inlined)
	if (InlinedTensor > -1)
	{
		InputSizeIfInlined = InputTensor.GetSizes();
	}
	// Copy memory (if not inlined)
	else
	{
		// Set parameters
		FCopyCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FCopyCS::FParameters>();
		Parameters->Num = OutputTensor.Num();
		// SRV/UAV variables
		Parameters->InputSRV = InputTensor.GetBufferSRVRef();
		Parameters->OutputUAV = OutputTensor.GetBufferUAVRef();
		// Set shader
		TShaderMapRef<FCopyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		// Run shader
		const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp(Parameters->Num, FCopyCS::THREADGROUP_SIZE_X);
		FComputeShaderUtils::AddPass(
			*InOutGraphBuilder,
			RDG_EVENT_NAME("FReshapeOperator-CopyCS()"),
			ComputeShader,
			Parameters,
			FIntVector(ThreadGroupCountValueX, 1, 1));
	}
	// Reshape output
	TArray<int64> OutputShape = FReshapeOperator::Remove0sAndMinus1sFromShape(ShapeTensor, InputTensor);
	OutputTensor.ReshapeMove(OutputShape);
}

void FReshapeOperator::PostForwardCPU()
{
	if (NeedsPostForward())
	{
		FNeuralTensor& InputTensor = *GetInputTensorsNoConst()[0];
		InputTensor.Reshape(InputSizeIfInlined);
	}
}

TArray<int64> FReshapeOperator::Remove0sAndMinus1sFromShape(const FNeuralTensor& InShapeTensor, const FNeuralTensor& InTensor) const
{
	TArray<int64> OutputShape = InShapeTensor.GetArrayCopy<int64>();
	const TArray<int64>& CurrentShape = InTensor.GetSizes();
	const int64 FinalVolume = InTensor.Num();

	int64 Minus1Index = -1;
	for (int64 ShapeIndex = 0; ShapeIndex < OutputShape.Num(); ++ShapeIndex)
	{
		// 0 = Copy from input
		if (OutputShape[ShapeIndex] == 0 && !AllowZero)
		{
			OutputShape[ShapeIndex] = CurrentShape[ShapeIndex];
		}
		// -1 = Infer from remaining dimensions
		else if (OutputShape[ShapeIndex] == -1)
		{
			Minus1Index = ShapeIndex;
			OutputShape[ShapeIndex] = 1;
		}
	}
	// Infer from remaining dimensions - All 0s must have been already converted into the right dimensions, and the only -1 turned into 1 for Product() to work properly
	if (Minus1Index > -1)
	{
		OutputShape[Minus1Index] = FinalVolume / FNeuralNetworkInferenceUtils::Product<int64>(OutputShape);
	}
	// Return cleaned OutputShape (without 0s or -1s)
	return OutputShape;
}
