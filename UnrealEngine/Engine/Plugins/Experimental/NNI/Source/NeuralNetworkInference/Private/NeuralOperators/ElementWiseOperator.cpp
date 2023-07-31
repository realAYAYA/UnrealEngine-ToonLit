// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ElementWiseOperator.h"
#include "ElementWiseCS.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"
#include "NeuralOperatorEnumClasses.h"



/* IElementWiseOperator structors
 *****************************************************************************/

IElementWiseOperator::IElementWiseOperator(const FString& InName, const int32 InVersion, const TSharedPtr<EElementWiseOperator>& InElementWiseOperator,
	const bool bIsInlinedTensor, const TArray<float>& InAttributes)
	: FNeuralOperator(InName, InVersion, (bIsInlinedTensor ? 0 : -1))
	, Attributes(InAttributes)
	, ElementWiseOperator(InElementWiseOperator)
{
}

IElementWiseOperator::~IElementWiseOperator()
{
}



/* IElementWiseOperator public functions
 *****************************************************************************/

bool IElementWiseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Attributes
	if (Attributes.Num() > 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IElementWiseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): So far only IElementWiseOperators with 0 or 1"
			" attributes, not implemented for %d."), Attributes.Num());
		return bIsLoaded;
	}
	// Input sanity checks
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(InputTensors, 1))
	{
		return bIsLoaded;
	}
	const FNeuralTensor& InputTensor = *InputTensors[0];
	// Output (if not inlined)
	if (InlinedTensor < 0)
	{
		TArray<FNeuralTensor*>& OutputTensors = GetOutputTensorsNoConst();
		// Sanity checks
		if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(OutputTensors, 1))
		{
			return bIsLoaded;
		}
		// Set and configure output tensor
		FNeuralTensor& OutputTensor = *OutputTensors.Last();
		OutputTensor.SetNumUninitialized(InputTensor.GetDataType(), InputTensor.GetSizes());
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

void IElementWiseOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IElementWiseOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
		return;
	}
	// Set parameters
	FElementWiseCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FElementWiseCS::FParameters>();
	// Input variables
	const FNeuralTensor& InputTensor = GetInputTensorConst();
	Parameters->TensorSize = (uint32)GetInputTensorConst().Num();
	if (Attributes.Num() > 0)
	{
		Parameters->Attribute = Attributes[0];
	}
	// SRV/UAV variables
	Parameters->InputSRV = InputTensor.GetBufferSRVRef();
	Parameters->OutputUAV = (InlinedTensor < 0 ? GetOutputTensorNoConst() : GetInputTensorNoConst()).GetBufferUAVRef(); // Not inlined vs. inlined
	// Set shader
	FElementWiseCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FElementWiseCS::FShaderType>(*ElementWiseOperator);
	PermutationVector.Set<FElementWiseCS::FIsInlined>((InlinedTensor > -1));
	TShaderMapRef<FElementWiseCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
	// Run shader
	const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp((uint32)InputTensor.Num(), FElementWiseCS::THREADGROUP_SIZE_X);
	FComputeShaderUtils::AddPass(
		*InOutGraphBuilder,
		RDG_EVENT_NAME("FElementWiseCS() - Operator: %s, InlinedTensor: %d", *Name, InlinedTensor),
		ComputeShader,
		Parameters,
		FIntVector(ThreadGroupCountValueX, 1, 1));
}



/* IElementWiseOperator protected functions
 *****************************************************************************/

void IElementWiseOperator::ForwardCPUWithFunction(float InOperatorFunction(const float))
{
	// Sanity checks
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IElementWiseOperator::ForwardCPUWithFunction(): bIsLoaded was false."));
		return;
	}
	if (Attributes.Num() != 0)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IElementWiseOperator::ForwardCPUWithFunction(): Attributes.Num() = %d != 0."), Attributes.Num());
		return;
	}
	// Not inlined layer (output is different than input)
	if (InlinedTensor < 0)
	{
		const FNeuralTensor& InputTensor = GetInputTensorConst();
		FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
		for (int64 Index = 0; Index < InputTensor.Num(); ++Index)
		{
			OutputTensor.At<float>(Index) = InOperatorFunction(InputTensor.At<float>(Index));
		}
	}
	// Inlined layer
	else
	{
		FNeuralTensor& InputTensor = GetInputTensorNoConst();
		for (int64 Index = 0; Index < InputTensor.Num(); ++Index)
		{
			float& TensorValue = InputTensor.At<float>(Index);
			TensorValue = InOperatorFunction(TensorValue);
		}
	}
}

void IElementWiseOperator::ForwardCPUWithFunction(float InOperatorFunction(const float, const float))
{
	// Sanity checks
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IElementWiseOperator::ForwardCPUWithFunction(): bIsLoaded was false."));
		return;
	}
	if (Attributes.Num() != 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IElementWiseOperator::ForwardCPUWithFunction(): Attributes.Num() = %d != 1."), Attributes.Num());
		return;
	}
	// Not inlined layer (output is different than input)
	if (InlinedTensor < 0)
	{
		const FNeuralTensor& InputTensor = GetInputTensorConst();
		FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
		for (int64 Index = 0; Index < InputTensor.Num(); ++Index)
		{
			OutputTensor.At<float>(Index) = InOperatorFunction(InputTensor.At<float>(Index), Attributes[0]);
		}
	}
	// Inlined layer
	else
	{
		FNeuralTensor& InputTensor = GetInputTensorNoConst();
		for (int64 Index = 0; Index < InputTensor.Num(); ++Index)
		{
			float& TensorValue = InputTensor.At<float>(Index);
			TensorValue = InOperatorFunction(TensorValue, Attributes[0]);
		}
	}
}
