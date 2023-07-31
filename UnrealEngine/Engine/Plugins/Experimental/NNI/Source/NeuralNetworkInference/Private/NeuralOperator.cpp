// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperator.h"
#include "NeuralNetworkInferenceUtils.h"



/* FPrivateNeuralOperator
 *****************************************************************************/

class FPrivateNeuralOperator
{
public:
	static void SanityCheckInlining(const bool bInShouldBeInlined, const int32 InInlinedTensor, const FString& InName);
};

void FPrivateNeuralOperator::SanityCheckInlining(const bool bInShouldBeInlined, const int32 InInlinedTensor, const FString& InName)
{
	checkf((bInShouldBeInlined && InInlinedTensor >= 0) || (!bInShouldBeInlined && InInlinedTensor < 0),
		TEXT("FNeuralOperator-%s::SanityCheckInlining(): bInShouldBeInlined (%d) vs. InInlinedTensor (%d) mismatch. If"
			" bInShouldBeInlined == true, InInlinedTensor should be >=0. If false, InInlinedTensor should be < 0. Possible reasons:"
			" 1) Non-const input called by a non-inlined operator;"
			" 2) Non-existing output called by an inlined operator;"
			" 3) SetDataType() was not called."),
		*InName, bInShouldBeInlined, InInlinedTensor, (int32)bInShouldBeInlined);
}



/* FNeuralOperator structors
 *****************************************************************************/

FNeuralOperator::FNeuralOperator(const FString& InName, const int32 InVersion, const int32 InInlinedTensor)
	: bIsLoaded(false)
	, Name(InName)
	, Version(InVersion)
	, InlinedTensor(InInlinedTensor)
{
}

FNeuralOperator::FNeuralOperator(const FString& InName, const int32 InVersion, const TSet<uint32>& InPotentialInlinedTensors)
	: bIsLoaded(false)
	, Name(InName)
	, Version(InVersion)
	, PotentiallyInlinedTensors(InPotentialInlinedTensors)
	, InlinedTensor(-1)
{
}

FNeuralOperator::~FNeuralOperator()
{
}



/* FNeuralOperator public functions
 *****************************************************************************/

int32 FNeuralOperator::SetInputTensorsAndGetInlinedIndex(const TArray<FNeuralTensor*>& InInputTensors)
{
	PrivateInputTensors = InInputTensors;
	if (!EstimateInlinedTensorFromPotentialOnes())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralOperator::SetInputTensorsAndGetInlinedIndex(): EstimateInlinedTensorFromPotentialOnes() returned false."));
		InlinedTensor = -2;
	}
	return InlinedTensor;
}

void FNeuralOperator::SetAuxiliaryTensors(const TArray<FNeuralTensor*>& InAuxiliaryTensors)
{
	checkf(GetMaximumPossibleNumberAuxiliaryTensorsPerOperator() >= InAuxiliaryTensors.Num(),
		TEXT("FNeuralOperator-%s::SetAuxiliaryTensors(): Either 1) This is a new operator so GetMaximumPossibleNumberAuxiliaryTensorsPerOperator() must be modified to return the new maximum (%d) or 2) InAuxiliaryTensors.Num() is too big for this operator."),
		*Name, InAuxiliaryTensors.Num());
	AuxiliaryTensors = InAuxiliaryTensors;
}

bool FNeuralOperator::Configure()
{
	// Trick to only run until one of them is false (or otherwise it runs them all and return true)
	return (ConfigureOutputAndInternalVariablesAndSanityChecks() && SetWhetherInputTensorsNeedTransferToGPUForGPUMode() && SetWhetherOutputTensorsNeedTransferToGPUForGPUMode() && SetWhetherAuxiliaryTensorsNeedTransferToGPUForGPUMode());
}

FString FNeuralOperator::ToString() const
{
	const FString InlinedText = (InlinedTensor < 0 ? TEXT("not inlined") : TEXT("inlined on tensor ") + FString::FromInt(InlinedTensor) );
	return FString::Format(TEXT("{0}_v{1} ({2})"), { Name, FString::FromInt(Version), InlinedText });
}



/* FNeuralOperator protected functions
 *****************************************************************************/

bool FNeuralOperator::SetWhetherInputTensorsNeedTransferToGPUForGPUMode()
{
	// Enable GPU mode on input tensors
	for (FNeuralTensor* PrivateInputTensor : PrivateInputTensors)
	{
		PrivateInputTensor->SetEnableGPU(true);
	}
	return true;
}

bool FNeuralOperator::SetWhetherOutputTensorsNeedTransferToGPUForGPUMode()
{
	// Enable GPU mode on output tensors
	for (FNeuralTensor* PrivateOutputTensor : PrivateOutputTensors)
	{
		PrivateOutputTensor->SetEnableGPU(true);
	}
	return true;
}

bool FNeuralOperator::SetWhetherAuxiliaryTensorsNeedTransferToGPUForGPUMode()
{
	// Enable GPU mode on auxiliary tensors
	for (FNeuralTensor* AuxiliaryTensor : AuxiliaryTensors)
	{
		AuxiliaryTensor->SetEnableGPU(true);
	}
	return true;
}

FNeuralTensor& FNeuralOperator::GetInputTensorNoConst()
{
	FPrivateNeuralOperator::SanityCheckInlining(true, InlinedTensor, Name);
	checkf(PrivateInputTensors.Num() == 1, TEXT("FNeuralOperator::GetInputTensorNoConst(): Required 1 input, not %d. Call GetInputTensorsNoConst() instead."), PrivateInputTensors.Num());
	return *PrivateInputTensors[0];
}

TArray<FNeuralTensor*>& FNeuralOperator::GetInputTensorsNoConst()
{
	FPrivateNeuralOperator::SanityCheckInlining(true, InlinedTensor, Name);
	return PrivateInputTensors;
}

const FNeuralTensor& FNeuralOperator::GetInputTensorConst() const
{
	checkf(PrivateInputTensors.Num() == 1, TEXT("FNeuralOperator::GetInputTensorConst(): Required 1 input, not %d. Call GetInputTensorsConst() instead."), PrivateInputTensors.Num());
	return *PrivateInputTensors[0];
}

FNeuralTensor& FNeuralOperator::GetOutputTensorNoConst()
{
	FPrivateNeuralOperator::SanityCheckInlining(false, InlinedTensor, Name);
	checkf(PrivateOutputTensors.Num() == 1, TEXT("FNeuralOperator::GetOutputTensorNoConst(): Required 1 output, not %d. Call GetOutputTensorsNoConst() instead."), PrivateOutputTensors.Num());
	return *PrivateOutputTensors[0];
}

TArray<FNeuralTensor*>& FNeuralOperator::GetOutputTensorsNoConst()
{
	FPrivateNeuralOperator::SanityCheckInlining(false, InlinedTensor, Name);
	return PrivateOutputTensors;
}

const FNeuralTensor& FNeuralOperator::GetOutputTensorConst() const
{
	FPrivateNeuralOperator::SanityCheckInlining(false, InlinedTensor, Name);
	checkf(PrivateOutputTensors.Num() == 1, TEXT("FNeuralOperator::GetOutputTensorConst(): Required 1 output, not %d. Call GetOutputTensorsConst() instead."), PrivateOutputTensors.Num());
	return *PrivateOutputTensors[0];
}

const TArray<FNeuralTensor*>& FNeuralOperator::GetOutputTensorsConst() const
{
	FPrivateNeuralOperator::SanityCheckInlining(false, InlinedTensor, Name);
	return PrivateOutputTensors;
}
