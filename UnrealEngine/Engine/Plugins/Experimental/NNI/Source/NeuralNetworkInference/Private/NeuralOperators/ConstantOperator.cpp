// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ConstantOperator.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"



/* FConstantOperator structors
 *****************************************************************************/

FConstantOperator::FConstantOperator(const FNodeProto* const InNodeProto)
	: FConstantOperator(FNeuralTensor())
{
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator(): InNodeProto was a nullptr."));
		return;
	}
	// Fill tensor from one of the attributes
	if (const FAttributeProto* SparseValueAttribute = FModelProto::FindElementInArray(TEXT("sparse_value"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"sparse_value\" not implemented yet."));
	}
	else if (const FAttributeProto* ValueAttribute = FModelProto::FindElementInArray(TEXT("value"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		if (ValueAttribute->Type == EAttributeProtoAttributeType::TENSOR)
		{
			// Get new tensor from TensorProto
			if (!Tensor.SetFromTensorProto(&ValueAttribute->T, TEXT("FConstantOperator-") + ValueAttribute->Name, ENeuralTensorType::Weight))
			{
				UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): SetFromTensorProto() failed."));
			}
		}
		// Sanity check
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): Not implemented type used, ValueAttribute->Type = %d."), (int32)ValueAttribute->Type);
		}
	}
	else if (const FAttributeProto* ValueFloatAttribute = FModelProto::FindElementInArray(TEXT("value_float"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"value\" not implemented yet."));
	}
	else if (const FAttributeProto* ValueFloatsAttribute = FModelProto::FindElementInArray(TEXT("value_floats"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"value_floats\" not implemented yet."));
	}
	else if (const FAttributeProto* ValueIntAttribute = FModelProto::FindElementInArray(TEXT("value_int"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"value_int\" not implemented yet."));
	}
	else if (const FAttributeProto* ValueIntsAttribute = FModelProto::FindElementInArray(TEXT("value_ints"), InNodeProto->Attribute,/*bMustValueBeFound*/ false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"value_ints\" not implemented yet."));
	}
	else if (const FAttributeProto* ValueStringAttribute = FModelProto::FindElementInArray(TEXT("value_string"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"value_string\" not implemented yet."));
	}
	else if (const FAttributeProto* ValueStringsAttribute = FModelProto::FindElementInArray(TEXT("value_strings"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): \"value_strings\" not implemented yet."));
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::FConstantOperator(): No valid attributes found in NodeProto."));
	}
}

FConstantOperator::FConstantOperator(const FNeuralTensor& InTensor)
	: FNeuralOperator(TEXT("Constant"), 13, -1)
	, Tensor(InTensor)
{
}

FConstantOperator::~FConstantOperator()
{
}



/* FConstantOperator public functions
 *****************************************************************************/

bool FConstantOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// InlinedTensor must be false
	if (InlinedTensor > -1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): InlinedTensor must be -1, but found %d."), InlinedTensor);
		return bIsLoaded;
	}
	// Input sanity checks
	const int32 NumberInputTensors = GetInputTensorsConst().Num();
	if (NumberInputTensors > 0)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): There cannot be any input tensors, but found %d."), NumberInputTensors);
		return bIsLoaded;
	}
	// Output sanity checks
	const int32 NumberOutputTensors = GetOutputTensorsConst().Num();
	if (NumberOutputTensors != 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): There must be exactly 1 output tensor, but found %d."), NumberOutputTensors);
		return bIsLoaded;
	}
	// Variable sanity check
	if (Tensor.IsEmpty())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Tensor cannot be empty."));
		return bIsLoaded;
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

void FConstantOperator::ForwardCPU()
{
	// Sanity check
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::ForwardCPU(): bIsLoaded was false."));
		return;
	}

	// Fill output tensor
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
	OutputTensor.SetNumUninitialized(Tensor.GetDataType(), Tensor.GetSizes());
	OutputTensor.SetFromUnderlyingUInt8ArrayCopy(Tensor.GetUnderlyingUInt8ArrayRef());
}

void FConstantOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConstantOperator::ForwardGPU_RenderThread(): Constant does not implement the GPU forward pass, only CPU one."));
}
