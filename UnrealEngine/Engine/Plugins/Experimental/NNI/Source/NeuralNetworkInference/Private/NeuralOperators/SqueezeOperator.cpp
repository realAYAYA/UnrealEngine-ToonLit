// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SqueezeOperator.h"
#include "CopyCS.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FSqueezeOperator structors
 *****************************************************************************/

FSqueezeOperator::FSqueezeOperator(const bool bIsInlinedTensor)
	: FNeuralOperator(TEXT("Squeeze"), 13, (bIsInlinedTensor ? 0 : -1))
{
}

FSqueezeOperator::~FSqueezeOperator()
{
}



/* FSqueezeOperator public functions
 *****************************************************************************/

bool FSqueezeOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Output initialization and size sanity check
	if (InlinedTensor < 0)
	{
		FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
		const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
		const FNeuralTensor& InputTensor = *InputTensors[0];

		// Initialize OutputNeuralTensor
		if (InputTensors.Num() > 1)
		{
			const FNeuralTensor& AxesTensor = *InputTensors[1];
			TArray<int64> OutputShape = FSqueezeOperator::Remove1sFromShape(InputTensor, AxesTensor);
			OutputTensor.SetNumUninitialized(InputTensor.GetDataType(), OutputShape);
		}
		else
		{
			TArray<int64> OutputShape = FSqueezeOperator::Remove1sFromShape(InputTensor);
			OutputTensor.SetNumUninitialized(InputTensor.GetDataType(), OutputShape);
		}
		
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

bool FSqueezeOperator::SetWhetherInputTensorsNeedTransferToGPUForGPUMode()
{
	// Enable GPU mode only on the first input tensor
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	InputTensors[0]->SetEnableGPU(true);
	return true;
}

void FSqueezeOperator::ForwardCPU()
{
	// Sanity check
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FSqueezeOperator::ForwardCPU(): bIsLoaded was false."));
		return;
	}
	// Squeeze is 0-cost if inlined, otherwise, just copy memory from input to output
	const TArray<FNeuralTensor*> InputTensors = GetInputTensorsConst();
	const FNeuralTensor& InputTensor = *InputTensors[0];
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
	TArray<int64> OutputShape;
	if (InputTensors.Num() > 1)
	{
		const FNeuralTensor& AxesTensor = *InputTensors[1];
		// Squeeze output
		OutputShape = FSqueezeOperator::Remove1sFromShape(InputTensor, AxesTensor);
	}
	else 
	{
		// Squeeze output
		OutputShape = FSqueezeOperator::Remove1sFromShape(InputTensor);
	}
	OutputTensor.ReshapeMove(OutputShape);
}

void FSqueezeOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FSqueezeOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
		return;
	}
	// Squeeze is 0-cost if inlined, otherwise, just copy memory from input to output
	const TArray<FNeuralTensor*> InputTensors = GetInputTensorsConst();
	const FNeuralTensor& InputTensor = *InputTensors[0];
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
			RDG_EVENT_NAME("FSqueezeeOperator-CopyCS()"),
			ComputeShader,
			Parameters,
			FIntVector(ThreadGroupCountValueX, 1, 1));
	}
	// Reshape output
	TArray<int64> OutputShape;
	if (InputTensors.Num() > 1)
	{
		const FNeuralTensor& AxesTensor = *InputTensors[1];
		// Squeeze output
		OutputShape = FSqueezeOperator::Remove1sFromShape(InputTensor, AxesTensor);
	}
	else
	{
		// Squeeze output
		OutputShape = FSqueezeOperator::Remove1sFromShape(InputTensor);
	}
	OutputTensor.ReshapeMove(OutputShape);
}

void FSqueezeOperator::PostForwardCPU()
{
	if (NeedsPostForward())
	{
		FNeuralTensor& InputTensor = *GetInputTensorsNoConst()[0];
		InputTensor.Reshape(InputSizeIfInlined);
	}
}

TArray<int64> FSqueezeOperator::Remove1sFromShape(const FNeuralTensor& InTensor, const FNeuralTensor& InAxesTensor) const
{
	const TArray<int64>& CurrentShape = InTensor.GetSizes();
	TArray<int64> OutputShape = CurrentShape;
	TArray<int64> AxesList = InAxesTensor.GetArrayCopy<int64>();

	// Convert negative indices to the equivalent positive indices
	for (int64& Axes : AxesList)
	{
		if (Axes < 0)
		{
			Axes += CurrentShape.Num();
		}
	}

	// Sort Axes in descending order
	AxesList.Sort([](const int64& A, const int64& B) {
		return A > B;
		});

	// Remove all duplicates
	bool isDuplicateRemoved = false;
	for (int64 CurrentAxesIndex = 0; CurrentAxesIndex < AxesList.Num() - 1; ++CurrentAxesIndex)
	{
		if (AxesList[CurrentAxesIndex] == AxesList[CurrentAxesIndex + 1])
		{
			AxesList.RemoveAt(CurrentAxesIndex);
			if (!isDuplicateRemoved)
			{
				isDuplicateRemoved = true;
			}
		}
	}

	if (isDuplicateRemoved)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FSqueezeOperator::Remove1sFromShape(): Axes input had duplicate(s) and was removed."));
	}

	for (int64 Axes : AxesList)
	{

		// 1 = remove from shape
		if (CurrentShape[Axes] == 1)
		{
			OutputShape.RemoveAt(Axes);
		}
		else 
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FSqueezeOperator::Remove1sFromShape(): Axes has an invalid index to remove (Shape value is not equal to one)."));
			TArray<int64> EmptyTArray;
			return EmptyTArray;
		}
	}

	// Return cleaned OutputShape (without 1s)
	return OutputShape;
}

TArray<int64> FSqueezeOperator::Remove1sFromShape(const FNeuralTensor& InTensor) const
{
	const TArray<int64>& CurrentShape = InTensor.GetSizes();
	TArray<int64> OutputShape = CurrentShape;

	for (int64 CurrentIndex = CurrentShape.Num() - 1; CurrentIndex > -1; --CurrentIndex)
	{
		// 1 = remove from shape
		if (CurrentShape[CurrentIndex] == 1)
		{
			OutputShape.RemoveAt(CurrentIndex);
		}
	}

	// Return cleaned OutputShape (without 1s)
	return OutputShape;
}