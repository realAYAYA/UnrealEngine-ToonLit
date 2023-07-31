// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/BatchNormalizationOperator.h"
#include "BatchNormalizationCS.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FPrivateBatchNormalizationOperator static functions
 *****************************************************************************/

class FPrivateBatchNormalizationOperator
{
public:
	static void ChannelBatchNormalization(FNeuralTensor& InOutOutputTensor, const TArray<FNeuralTensor*>& InInputTensors, const int64 InChannelIndex, const int64 InImageArea,
		const int64 InAbsoluteIndexBias, const float InEpsilon);
};

void FPrivateBatchNormalizationOperator::ChannelBatchNormalization(FNeuralTensor& InOutOutputTensor, const TArray<FNeuralTensor*>& InInputTensors, const int64 InChannelIndex,
	const int64 InImageArea, const int64 InAbsoluteIndexBias, const float InEpsilon)
{
	const float Scale = InInputTensors[1]->At<float>(InChannelIndex);
	const float Bias = InInputTensors[2]->At<float>(InChannelIndex);
	const float Mean = InInputTensors[3]->At<float>(InChannelIndex);
	const float Variance = InInputTensors[4]->At<float>(InChannelIndex);
	for (int64 AreaIndex = 0; AreaIndex < InImageArea; ++AreaIndex)
	{
		const int64 AbsoluteIndex = InAbsoluteIndexBias + AreaIndex;
		const float X = InInputTensors[0]->At<float>(AbsoluteIndex);
		InOutOutputTensor.At<float>(AbsoluteIndex) = Scale * (X - Mean) / FMath::Sqrt(Variance + InEpsilon) + Bias;
	}
}



/* FBatchNormalizationOperator structors
 *****************************************************************************/

FBatchNormalizationOperator::FBatchNormalizationOperator(const bool bIsInlinedTensor, const FNodeProto* const InNodeProto)
	: FBatchNormalizationOperator(bIsInlinedTensor)
{
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FBatchNormalizationOperator(): InNodeProto was a nullptr."));
		return;
	}
	if (const FAttributeProto* EpsilonAttribute = FModelProto::FindElementInArray(TEXT("Epsilon"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Epsilon = EpsilonAttribute->F;
	}
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("Momentum"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Momentum = MomentumAttribute->F;
	}
}

FBatchNormalizationOperator::FBatchNormalizationOperator(const bool bIsInlinedTensor, const float InEpsilon, const float InMomentum)
	: FNeuralOperator(TEXT("BatchNormalization"), 9, (bIsInlinedTensor ? 0 : -1))
	, Epsilon(InEpsilon)
	, Momentum(InMomentum)
{
}

FBatchNormalizationOperator::~FBatchNormalizationOperator()
{
}



/* FBatchNormalizationOperator public functions
 *****************************************************************************/

bool FBatchNormalizationOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Input sanity checks
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(InputTensors, 5))
	{
		return bIsLoaded;
	}
	for (int32 InputTensorIndex = 0; InputTensorIndex < InputTensors.Num(); ++InputTensorIndex)
	{
		FNeuralTensor& InputTensor = *InputTensors[InputTensorIndex];
		if (InputTensor.GetNumberDimensions() != 1 && (InputTensorIndex != 0 || InputTensor.GetNumberDimensions() < 1))
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FBatchNormalizationOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): InputTensor.GetNumberDimensions() is %d and not 1 (or also greater than 2 for index 0) failed for tensor index %d and name %s."),
				InputTensor.GetNumberDimensions(), InputTensorIndex, *InputTensor.GetName());
			return bIsLoaded;
		}
	}
	// Output (if not inlined)
	if (InlinedTensor < 0)
	{
		if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(GetOutputTensorsConst(), 1))
		{
			return bIsLoaded;
		}
		FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
		if (OutputTensor.GetSizes() != InputTensors[0]->GetSizes())
		{
			OutputTensor.SetNumUninitialized(InputTensors[0]->GetDataType(), InputTensors[0]->GetSizes());
		}
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

void FBatchNormalizationOperator::ForwardCPU()
{
	// Sanity check
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FBatchNormalizationOperator::ForwardCPU(): bIsLoaded was false."));
		return;
	}

	// Get input and output tensors
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	FNeuralTensor& OutputTensor = (InlinedTensor < 0 ? GetOutputTensorNoConst() : *GetInputTensorsNoConst()[0]);

	// Apply batch normalization
	// > 1 dimensions
	if (OutputTensor.GetNumberDimensions() > 1)
	{
		const int64 BatchSize = OutputTensor.GetSize(0);
		const int64 ChannelSize = OutputTensor.GetSize(1);
		const int64 ChannelsVolume = OutputTensor.Num() / BatchSize;
		const int64 ImageArea = ChannelsVolume / ChannelSize;
		// Per batch
		for (int64 NIndex = 0; NIndex < BatchSize; ++NIndex)
		{
			// Per channel
			for (int64 ChannelIndex = 0; ChannelIndex < ChannelSize; ++ChannelIndex)
			{
				const int64 AbsoluteIndexBias = NIndex * ChannelsVolume + ChannelIndex * ImageArea;
				FPrivateBatchNormalizationOperator::ChannelBatchNormalization(OutputTensor, InputTensors, ChannelIndex, ImageArea, AbsoluteIndexBias, Epsilon);
			}
		}
	}
	// 1 dimension
	else if (OutputTensor.GetNumberDimensions() == 1)
	{
		FPrivateBatchNormalizationOperator::ChannelBatchNormalization(OutputTensor, InputTensors, /*Channel*/0, /*ImageArea*/OutputTensor.Num(), /*AbsoluteIndexBias*/0, Epsilon);
	}
	// Unknown
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FBatchNormalizationOperator::ForwardCPU(): OutputTensor.GetNumberDimensions() = %d but should be >= 1."), OutputTensor.GetNumberDimensions());
	}
}

void FBatchNormalizationOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FBatchNormalizationOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
		return;
	}
	// Set parameters
	FNeuralTensor& OutputTensor = (InlinedTensor < 0 ? GetOutputTensorNoConst() : *GetInputTensorsNoConst()[0]);
	const EBatchNormalizationMode BatchNormalizationMode = (OutputTensor.GetNumberDimensions() > 1 ? EBatchNormalizationMode::NDimensions : EBatchNormalizationMode::OneDimension);
	FBatchNormalizationCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FBatchNormalizationCS::FParameters>();
	// > 1 dimensions
	if (BatchNormalizationMode == EBatchNormalizationMode::NDimensions)
	{
		Parameters->BatchSize = OutputTensor.GetSize(0);
		Parameters->ChannelSize = OutputTensor.GetSize(1);
		Parameters->ChannelsVolume = OutputTensor.Num() / Parameters->BatchSize;
		Parameters->ImageArea = Parameters->ChannelsVolume / Parameters->ChannelSize;
	}
	else
	{
		Parameters->ImageArea = OutputTensor.Num();
	}
	Parameters->Epsilon = Epsilon;
	// SRV/UAV variables
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	const bool bIsInlined = (InlinedTensor > -1);
	if (!bIsInlined)
	{
		Parameters->XSRV = InputTensors[0]->GetBufferSRVRef();
	}
	Parameters->OutputUAV = OutputTensor.GetBufferUAVRef();
	Parameters->Scale = InputTensors[1]->GetBufferSRVRef();
	Parameters->Bias = InputTensors[2]->GetBufferSRVRef();
	Parameters->Mean = InputTensors[3]->GetBufferSRVRef();
	Parameters->Variance = InputTensors[4]->GetBufferSRVRef();
	// Set shader
	FBatchNormalizationCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FBatchNormalizationCS::FBatchNormalizationMode>(BatchNormalizationMode);
	PermutationVector.Set<FBatchNormalizationCS::FIsInlined>(bIsInlined);
	TShaderMapRef<FBatchNormalizationCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
	// Run shader
	const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp(Parameters->ImageArea, FBatchNormalizationCS::THREADGROUP_SIZE_X);
	FComputeShaderUtils::AddPass(
		*InOutGraphBuilder,
		RDG_EVENT_NAME("FBatchNormalizationCS() - Operator: %s, EBatchNormalizationMode: %d", *Name, BatchNormalizationMode),
		ComputeShader,
		Parameters,
		FIntVector(ThreadGroupCountValueX, 1, 1));
}
