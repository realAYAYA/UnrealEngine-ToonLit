// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ConvBaseOperator.h"
#include "ConvBaseCS.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FPrivateConvBaseOperator auxiliary functions
 *****************************************************************************/

class FPrivateConvBaseOperator
{
public:
	static void NDTensorIndexesPlus1(TArray<int64>& InOutImageAreaIndexes, const TArray<int64>& InSizes);

	/**
	 * It will return an empty FNeuralInt64ArrayUInt32Buffer if any error occurred.
	 */
	static FNeuralInt64ArrayUInt32Buffer TransposePadsFromConvPads(const FNeuralInt64ArrayUInt32Buffer& InPads, const FConvBaseOperator::EAutoPad InAutoPad, const TArray<int64>& InEffectiveKernelSizes);
};

void FPrivateConvBaseOperator::NDTensorIndexesPlus1(TArray<int64>& InOutImageAreaIndexes, const TArray<int64>& InSizes)
{
	int64 ImageAreaIndexesIndex = InSizes.Num() - 3; // = NumberConvolutionalDimensions - 1 = InSizes.Num() - 2 - 1
	while (ImageAreaIndexesIndex > -1)
	{
		++InOutImageAreaIndexes[ImageAreaIndexesIndex];
		if (InOutImageAreaIndexes[ImageAreaIndexesIndex] == InSizes[ImageAreaIndexesIndex + 2])
		{
			InOutImageAreaIndexes[ImageAreaIndexesIndex] = 0;
			--ImageAreaIndexesIndex;
		}
		else
		{
			break;
		}
	}
}

FNeuralInt64ArrayUInt32Buffer FPrivateConvBaseOperator::TransposePadsFromConvPads(const FNeuralInt64ArrayUInt32Buffer& InPads, const FConvBaseOperator::EAutoPad InAutoPad, const TArray<int64>& InEffectiveKernelSizes)
{
	FNeuralInt64ArrayUInt32Buffer Pads;
	TArray<int64> PadsArrayTemporary;
	PadsArrayTemporary.SetNumUninitialized(InPads.Num());
	for (int32 Index = 0; Index < PadsArrayTemporary.Num(); ++Index)
	{
		PadsArrayTemporary[Index] = InEffectiveKernelSizes[Index / 2] - InPads[Index] - 1;
		if (PadsArrayTemporary[Index] < 0)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator: PadsArrayTemporary[%d] = %d, ConvTranspose only implemented for PadsArrayTemporary[%d] >= 0 for now."), Index, PadsArrayTemporary[Index], Index);
			return Pads;
		}
	}
	Pads.Move(PadsArrayTemporary);
	return Pads;
}



/* FConvBaseOperator structors
 *****************************************************************************/

FConvBaseOperator::FConvBaseOperator(const FString& InName, const int32 InVersion, const int32 InInlinedTensor,
	const EAutoPad InAutoPad, const TArray<int64>& InDilations, const int64 InGroup, const TArray<int64>& InKernelShape, const TArray<int64>& InPads, const TArray<int64>& InStrides,
	const bool bInIsTransposed, const TArray<int64>& InOutputPadding, const TArray<int64>& InOutputShape)
	: FNeuralOperator(InName, InVersion, InInlinedTensor)
	, bIsTransposed(bInIsTransposed)
	, AutoPad(InAutoPad)
	, Dilations(InDilations)
	, Group(InGroup)
	, KernelShape(InKernelShape)
	, Pads(InPads)
	, Strides(InStrides)
	, OutputPadding(InOutputPadding)
	, OutputShape(InOutputShape)
{
}

FConvBaseOperator::~FConvBaseOperator()
{
}



/* FConvBaseOperator public functions
 *****************************************************************************/

bool FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Input sanity checks
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	const TArray<int64>& XSizes = InputTensors[0]->GetSizes();
	const TArray<int64>& WSizes = InputTensors[1]->GetSizes();
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(InputTensors, 2, 3, 3, -1, 0, 1) || !FNeuralNetworkInferenceUtils::SizeSanityChecks(InputTensors, 2, 3, 1, 1, 2, 2))
	{
		return false;
	}
	// Initialize and set first 2 dimensions of PartiallySetOutputSizes + sanity checks
	TArray<int64> PartiallySetOutputSizes;
	{
		if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(GetOutputTensorsConst(), 1, 1, -1, -1))
		{
			return bIsLoaded;
		}
		PartiallySetOutputSizes.SetNumUninitialized(XSizes.Num());
		PartiallySetOutputSizes[0] = XSizes[0];
		PartiallySetOutputSizes[1] = (!bIsTransposed ? WSizes[0] : Group * WSizes[1]);
	}
	// Sanity checks
	if (!SanityChecksForTensors(PartiallySetOutputSizes))
	{
		return bIsLoaded;
	}
	const int32 NumberDimensions = InputTensors[1]->GetNumberDimensions();
	const int32 NumberConvolutionalDimensions = NumberDimensions - 2;
	// Attribute sanity checks
	// If KernelShape given, make sure it matches W
	if (KernelShape.Num() > 0)
	{
		if (KernelShape.Num() != NumberConvolutionalDimensions)
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): KernelShape.Num() == NumberConvolutionalDimensions failed (%d != %d)."),
				KernelShape.Num(), NumberConvolutionalDimensions);
			return bIsLoaded;
		}
		for (int64 DimensionIndex = 2; DimensionIndex < NumberDimensions; ++DimensionIndex)
		{
			if (WSizes[DimensionIndex] != KernelShape[DimensionIndex - 2])
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): KernelShape[%d] does not match the actual size of W[%d] (%d != %d)."),
					DimensionIndex, DimensionIndex - 2, WSizes[DimensionIndex], KernelShape[DimensionIndex - 2]);
				return bIsLoaded;
			}
		}
	}
	// OutputPadding sanity check
	if (OutputPadding.Num() != 0 && OutputPadding.Num() != NumberConvolutionalDimensions)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): OutputPadding.Num() should be 0 or %d, but not %d."), NumberConvolutionalDimensions, OutputPadding.Num());
		return bIsLoaded;
	}
	// OutputShape sanity check
	if (OutputShape.Num() != 0 && OutputShape.Num() != NumberConvolutionalDimensions)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): OutputShape.Num() should be 0 or %d, but not %d."), NumberConvolutionalDimensions, OutputShape.Num());
		return bIsLoaded;
	}
	// OutputShape/Pads sanity check --> Only 1 of the 2
	if (OutputShape.Num() > 0 && Pads.Num() > 0)
	{
		for (int64 DimensionIndex = 0; DimensionIndex < FMath::Min(NumberConvolutionalDimensions, Pads.Num()); ++DimensionIndex) // Min to avoid crash if wrong Pad (wrong Pad checked later)
		{
			if (OutputShape[DimensionIndex] > 0 && Pads[DimensionIndex] > 0)
			{
				UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): You can only set OutputShape or Pads, but not both. Pads will be overridden."));
				Pads = FNeuralInt64ArrayUInt32Buffer();
				break;
			}
		}
	}
	if (!FillOutputShape(XSizes))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): FillOutputShape() failed."));
		return bIsLoaded;
	}
	// Set Dilations
	if (!FillDilationsIfEmpty(NumberConvolutionalDimensions))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): FillDilationsIfEmpty() failed."));
		return bIsLoaded;
	}
	// Set Strides
	if (!SetAndConfigureStrides(NumberConvolutionalDimensions))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): SetAndConfigureStrides() failed."));
		return bIsLoaded;
	}
	// Strides sanity checks
	if (Strides.Num() > 0 && Strides.Num() != NumberConvolutionalDimensions) // Right dimensions
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Strides.Num() == 0 || Strides.Num() == NumberConvolutionalDimensions failed (%d != 0 || %d)."),
			Strides.Num(), NumberConvolutionalDimensions);
		return bIsLoaded;
	}
	// Set AuxiliaryTensors if needed
	if (!SetAndConfigureAuxiliaryTensor())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): SetAndConfigureAuxiliaryTensor() failed."));
		return bIsLoaded;
	}
	for (const int64 Stride : Strides) // Strides >= 1 on each dimension
	{
		if (Stride < 1)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Stride (%d) < 1 found! Stride must be >= 1."), Stride);
			return bIsLoaded;
		}
	}
	// Create EffectiveKernelSizes
	const TArray<int64> EffectiveKernelSizes = CreateEffectiveKernelSizes(NumberConvolutionalDimensions, WSizes);
	// Either AutoPad == NotSet with explicit Pads or AutoPad != NotSet without explicit Pads. Both together is not possible
	if (AutoPad != EAutoPad::NotSet && Pads.Num() != 0)
	{
		// Make sure Pads != {0,0,...,0} (or it would be equivalent to Pads.Num() = 0, thus valid)
		for (const int64 Pad : Pads)
		{
			if (Pad != 0)
			{
				UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): If AutoPad (%d) is different than NotSet (%d), explicit padding is not allowed."),
					(int32)AutoPad, (int32)EAutoPad::NotSet);
				return bIsLoaded;
			}
		}
	}
	// Sanity check
	if (AuxiliaryTensors.Num() > 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureAuxiliaryTensor(): AuxiliaryTensors.Num() should be 0 or 1, not %d."), AuxiliaryTensors.Num());
		return false;
	}
	// Set Pads (based on InPads and InAutoPad)
	const FNeuralTensor& XOrXWithZeros = *(AuxiliaryTensors.Num() == 0 ? InputTensors[0] : AuxiliaryTensors[0]);
	const TArray<int64>& XOrXWithZerosSizes = XOrXWithZeros.GetSizes();
	if (!EstimateAndFillPads(EffectiveKernelSizes, NumberDimensions, XSizes))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): EstimateAndFillPads() failed."));
		return bIsLoaded;
	}
	// Pads size sanity checks
	if (Pads.Num() > 0 && Pads.Num() != 2 * NumberConvolutionalDimensions) // Right dimensions
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FConvOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Pads.Num() == 0 || Pads.Num() == 2 * NumberConvolutionalDimensions failed (%d != 0 || %d)."),
			Pads.Num(), 2 * NumberConvolutionalDimensions);
		return bIsLoaded;
	}
	for (const int64 Pad : Pads) // Pads >= 0 on each dimension
	{
		if (Pad < 0)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Pads >= 0 failed (found %d)."), Pad);
			return bIsLoaded;
		}
	}
	// Final sanity checks
	if (!FillOutputSizesAndPaddedMarginsAndSanityChecks(PartiallySetOutputSizes, EffectiveKernelSizes))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): FillOutputSizesAndPaddedMarginsAndSanityChecks() failed."));
		return bIsLoaded;
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

void FConvBaseOperator::ToGPU_RenderThread()
{
	// Sanity check
	if (OutputSizesForGPU.IsReadBufferValid() || XOrXWithZerosSizesForGPU.IsReadBufferValid() || WSizesForGPU.IsReadBufferValid()
		|| Dilations.IsReadBufferValid() || Strides.IsReadBufferValid() || Pads.IsReadBufferValid())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ToGPU_RenderThread():"
			" OutputSizesForGPU, XOrXWithZerosSizesForGPU, WSizesForGPU, OutputSizesForGPU, XOrXWithZerosSizesForGPU, WSizesForGPU, Dilations, Strides, and/or Pads FReadBuffers were not nullptr."));
	}
	// Get input and output tensors
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
	// Memory to GPU
	OutputSizesForGPU.Copy(OutputTensor.GetSizes());
	const FNeuralTensor& XOrXWithZeros = *(AuxiliaryTensors.Num() == 0 ? InputTensors[0] : AuxiliaryTensors[0]);
	XOrXWithZerosSizesForGPU.Copy(XOrXWithZeros.GetSizes());
	WSizesForGPU.Copy(InputTensors[1]->GetSizes());
	OutputSizesForGPU.CreateAndLoadSRVBuffer(TEXT("Conv_ReadBufferOutputSizes"));
	XOrXWithZerosSizesForGPU.CreateAndLoadSRVBuffer(TEXT("Conv_ReadBufferXSizes"));
	WSizesForGPU.CreateAndLoadSRVBuffer(TEXT("Conv_ReadBufferWSizes"));
	Dilations.CreateAndLoadSRVBuffer(TEXT("Conv_ReadBufferDilations"));
	Strides.CreateAndLoadSRVBuffer(TEXT("Conv_ReadBufferStrides"));
	Pads.CreateAndLoadSRVBuffer(TEXT("Conv_ReadBufferPads"));
}

void FConvBaseOperator::ForwardCPU()
{
	// Sanity check
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ForwardCPU(): bIsLoaded was false."));
		return;
	}

	// Get input and output tensors
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();

	// Set output to bias (or 0)
	if (InputTensors.Num() > 2)
	{
		const TArray<int64>& OutputSizes = OutputTensor.GetSizes();
		const int64 OutputBatchVolume = OutputTensor.Num() / OutputSizes[0];
		const int64 OutputImageArea = OutputBatchVolume / OutputSizes[1];
		const FNeuralTensor& B = *InputTensors[2];
		// Per batch N
		for (int64 BatchIndex = 0; BatchIndex < OutputSizes[0]; ++BatchIndex)
		{
			// Per output channel M
			for (int64 OutputChannelIndex = 0; OutputChannelIndex < OutputSizes[1]; ++OutputChannelIndex)
			{
				// Per image area (D1 x D2 x ... x Dn)
				for (int64 OutputImageIndex = 0; OutputImageIndex < OutputImageArea; ++OutputImageIndex)
				{
					OutputTensor.At<float>(BatchIndex * OutputBatchVolume + OutputChannelIndex * OutputImageArea + OutputImageIndex) = B.At<float>(OutputChannelIndex);
				}
			}
		}
	}
	else
	{
		OutputTensor.SetTo<float>(0);
	}

	// Apply convolution
	const FNeuralTensor& W = *InputTensors[1];
	const FNeuralTensor& XOrXWithZeros = *(AuxiliaryTensors.Num() == 0 ? InputTensors[0] : AuxiliaryTensors[0]);
	MultiChannelConvolutionCPU(OutputTensor, XOrXWithZeros, W);
}

void FConvBaseOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
		return;
	}
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
	const int32 NumberConvolutionalDimensions = OutputTensor.GetNumberDimensions() - 2;
	if (NumberConvolutionalDimensions > (int32)FConvBaseCS::MAX_NUMBER_DIMENSIONS)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ForwardGPU_RenderThread(): NumberConvolutionalDimensions <= FConvBaseCS::MAX_NUMBER_DIMENSIONS failed (%d vs. %d). I.e.,"
			" the maximum number of dimensions for FConvBaseOperator is FConvBaseCS::MAX_NUMBER_DIMENSIONS. Increase FConvBaseCS::MAX_NUMBER_DIMENSIONS if you need a higher dimensional convolution,"
			" which might result in a performance hit for all convolutions of more than 3 dimensions."),
			NumberConvolutionalDimensions, FConvBaseCS::MAX_NUMBER_DIMENSIONS);
		return;
	}
	// Set parameters
	FConvBaseCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FConvBaseCS::FParameters>();
	// Input SRV variables - Fill ReadBuffer's the first time
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	// Input SRV variables
	Parameters->OutputSizes = OutputSizesForGPU.GetUInt32SRV();
	Parameters->XOrXWithZerosSizes = XOrXWithZerosSizesForGPU.GetUInt32SRV();
	Parameters->WSizes = WSizesForGPU.GetUInt32SRV();
	Parameters->Dilations = Dilations.GetUInt32SRV();
	Parameters->Strides = Strides.GetUInt32SRV();
	Parameters->Pads = Pads.GetUInt32SRV();
	// Input variables
	Parameters->bIsTransposed = bIsTransposed;
	Parameters->Group = Group;
	Parameters->MIntoGroup = (!bIsTransposed ? WSizesForGPU[0] / Group : WSizesForGPU[1]);
	Parameters->CIntoGroup = WSizesForGPU[0] / Group;
	Parameters->OutputVolume = OutputTensor.Num();
	Parameters->OutputBatchVolume = Parameters->OutputVolume / OutputSizesForGPU[0];
	Parameters->OutputImageArea = Parameters->OutputBatchVolume / OutputSizesForGPU[1];
	Parameters->WBatchVolume = InputTensors[1]->Num() / WSizesForGPU[0];
	Parameters->WImageArea = Parameters->WBatchVolume / WSizesForGPU[1];
	const FNeuralTensor& XOrXWithZeros = *(AuxiliaryTensors.Num() == 0 ? InputTensors[0] : AuxiliaryTensors[0]);
	Parameters->XOrXWithZerosBatchVolume = XOrXWithZeros.Num() / XOrXWithZerosSizesForGPU[0];
	Parameters->XOrXWithZerosImageArea = Parameters->XOrXWithZerosBatchVolume / XOrXWithZerosSizesForGPU[1];
	// nD convolution
	const EConvMode ConvMode = (NumberConvolutionalDimensions == 1 ? EConvMode::D1
		: NumberConvolutionalDimensions == 2 ? EConvMode::D2
		: NumberConvolutionalDimensions == 3 ? EConvMode::D3
		: /*NumberConvolutionalDimensions > 3 ?*/ EConvMode::DN);
	if (ConvMode == EConvMode::DN)
	{
		Parameters->NumberConvolutionalDimensions = NumberConvolutionalDimensions;
	}
	// SRV/UAV variables
	Parameters->OutputUAV = OutputTensor.GetBufferUAVRef();
	Parameters->XOrXWithZerosSRV = XOrXWithZeros.GetBufferSRVRef(); // InputTensors[0] if normal convolution
	Parameters->WSRV = InputTensors[1]->GetBufferSRVRef();
	// Set output to bias
	const bool bHasBias = (InputTensors.Num() > 2);
	if (bHasBias)
	{
		Parameters->BSRV = InputTensors[2]->GetBufferSRVRef();
	}
	// Set shader
	FConvBaseCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FConvBaseCS::FHasBias>(bHasBias);
	PermutationVector.Set<FConvBaseCS::FConvMode>(ConvMode);
	TShaderMapRef<FConvBaseCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
	// Run shader
	const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp(Parameters->OutputVolume, FConvBaseCS::THREADGROUP_SIZE_X);
	FComputeShaderUtils::AddPass(
		*InOutGraphBuilder,
		RDG_EVENT_NAME("FConvBaseCS() - %dD convolution, Operator: %s", NumberConvolutionalDimensions, *Name),
		ComputeShader,
		Parameters,
		FIntVector(ThreadGroupCountValueX, 1, 1));
}



/* FConvBaseOperator private functions
 *****************************************************************************/

bool FConvBaseOperator::SanityChecksForTensors(const TArray<int64>& InPartiallySetOutputSizes) const
{
	// InlinedTensor should be false
	if (InlinedTensor > -1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::SanityChecksForTensors(): This operator cannot be inlined."));
		return false;
	}
	// Input sanity checks
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	const TArray<int64>& XSizes = InputTensors[0]->GetSizes();
	const TArray<int64>& WSizes = InputTensors[1]->GetSizes();
	if (InputTensors.Num() > 2)
	{
		if (!bIsTransposed)
		{
			if (WSizes[0] != InputTensors[2]->Num())
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FConvBaseOperator::SanityChecksForTensors(): InputTensors[1].GetSize(0) == InputTensors[2].Num() failed (%d != %d)."),
					WSizes[0], InputTensors[2]->Num());
				return false;
			}
		}
		else
		{
			if (Group * WSizes[1] != InputTensors[2]->Num())
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FConvBaseOperator::SanityChecksForTensors(): Group x InputTensors[1].GetSize(1) == InputTensors[2].Num() failed (%d x %d != %d)."),
					Group, WSizes[1], InputTensors[2]->Num());
				return false;
			}
		}
	}
	// Grouped convolution checks
	if (XSizes[1] % Group != 0 || WSizes[0] % Group != 0)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FConvBaseOperator::SanityChecksForTensors(): XSizes[1] %% Group == 0 && WSizes[0] %% Group == 0 failed (%d %% %d = %d && %d %% %d = %d)."),
			XSizes[1], Group, XSizes[1] % Group,
			WSizes[0], Group, WSizes[0] % Group);
		return false;
	}
	// Check C matches: XWithZeros = {B, C, ...}, W = {M, C/Group, ...}
	if (!bIsTransposed)
	{
		if (XSizes[1] != Group * WSizes[1])
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FConvBaseOperator::SanityChecksForTensors(): XSizes[1] == Group * WSizes[1] failed (%d != %d x %d)."),
				XSizes[1], Group, WSizes[1]);
			return false;
		}
	}
	else
	{
		if (XSizes[1] != WSizes[0])
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FConvBaseOperator::SanityChecksForTensors(): XSizes[1] == WSizes[0] failed (%d != %d)."),
				XSizes[1], WSizes[0]);
			return false;
		}
	}
	// XWithZeros, W, Output compatible to each other
	if (InPartiallySetOutputSizes.Num() != InputTensors[0]->GetNumberDimensions() || InPartiallySetOutputSizes.Num() != InputTensors[1]->GetNumberDimensions())
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FConvBaseOperator::SanityChecksForTensors(): InPartiallySetOutputSizes.Num() == InputTensors[0]->GetNumberDimensions() == InputTensors[1]->GetNumberDimensions() failed (%d vs. %d vs. %d)."),
			InPartiallySetOutputSizes.Num(), InputTensors[0]->GetNumberDimensions(), InputTensors[1]->GetNumberDimensions());
		return false;
	}
	// Check Output - Input sizes matches: XWithZeros = {B, C, ...}, W = {M, C/Group, ...}, Output = {N, M, ...}
	if (InPartiallySetOutputSizes[0] != XSizes[0])
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FConvBaseOperator::SanityChecksForTensors(): InPartiallySetOutputSizes[0] == XSizes[0] failed (%d != %d)."),
			InPartiallySetOutputSizes[0], XSizes[0]);
		return false;
	}
	if (!bIsTransposed)
	{
		if (InPartiallySetOutputSizes[1] != WSizes[0])
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FConvBaseOperator::SanityChecksForTensors(): InPartiallySetOutputSizes[1] == WSizes[0] failed (%d != %d)."),
				InPartiallySetOutputSizes[1], WSizes[0]);
			return false;
		}
	}
	else if (InPartiallySetOutputSizes[1] != Group * WSizes[1])
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FConvBaseOperator::SanityChecksForTensors(): InPartiallySetOutputSizes[1] == Group * WSizes[1] failed (%d != %d x %d)."),
			InPartiallySetOutputSizes[1], Group, WSizes[1]);
		return false;
	}
	return true;
}

bool FConvBaseOperator::FillOutputShape(const TArray<int64>& InXSizes)
{
	// Only applies for transposed convolution
	if (bIsTransposed)
	{
		const int32 NumberConvolutionalDimensions = InXSizes.Num() - 2;
		// If AutoPad == Same(Lower/Upper), fill OutputShape
		if (AutoPad == FConvBaseOperator::EAutoPad::SameLower || AutoPad == FConvBaseOperator::EAutoPad::SameUpper)
		{
			// Set OutputShapeTemporary
			TArray<int64> OutputShapeTemporary;
			OutputShapeTemporary.SetNumUninitialized(NumberConvolutionalDimensions);
			for (int64 DimensionIndex = 0; DimensionIndex < NumberConvolutionalDimensions; ++DimensionIndex)
			{
				OutputShapeTemporary[DimensionIndex] = InXSizes[DimensionIndex + 2] * (Strides.Num() > DimensionIndex ? Strides[DimensionIndex] : 1);
			}
			// Set OutputShape from OutputShapeTemporary
			if (OutputShape.IsEmpty())
			{
				OutputShape = OutputShapeTemporary;
			}
			// Sanity check
			else if (OutputShape != OutputShapeTemporary)
			{
				UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::FillOutputShape(): OutputShape == OutputShapeTemporary failed, %s != %s."),
					*FNeuralTensor(OutputShape).ToString(/*MaxNumberElementsToDisplay*/-1, /*bReturnOnlyData*/true), *FNeuralTensor(OutputShapeTemporary).ToString(/*MaxNumberElementsToDisplay*/-1, /*bReturnOnlyData*/true));
				return false;
			}
		}
	}
	return true;
}

bool FConvBaseOperator::FillDilationsIfEmpty(const int32 InNumberConvolutionalDimensions)
{
	// Create and fill InOutDilations (which might be empty)
	if (Dilations.Num() < 1)
	{
		Dilations.Init(1, InNumberConvolutionalDimensions, /*bShouldCreate64BitVersion*/true);
	}
	// Dilations sanity checks
	if (Dilations.Num() > 0 && Dilations.Num() != InNumberConvolutionalDimensions) // Right dimensions
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::FillDilationsIfEmpty(): Dilations.Num() == 0 || Dilations.Num() == InNumberConvolutionalDimensions failed (%d != 0 || %d)."),
			Dilations.Num(), InNumberConvolutionalDimensions);
		return false;
	}
	for (const int64 Dilation : Dilations) // Dilation >= 1 on each dimension
	{
		if (Dilation < 1)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::FillDilationsIfEmpty(): Dilation (%d) < 1 found! Dilation must be >= 1."), Dilation);
			return false;
		}
	}
	return true;
}

TArray<int64> FConvBaseOperator::CreateEffectiveKernelSizes(const int32 InNumberConvolutionalDimensions, const TArray<int64>& InWSizes)
{
	// Set EffectiveKernelSizes, whose size with dilation is: k_prime = k + (k-1)(d-1) = d(k-1)+1
	TArray<int64> EffectiveKernelSizes;
	EffectiveKernelSizes.SetNumUninitialized(InNumberConvolutionalDimensions);
	for (int64 DimensionIndex = 0; DimensionIndex < InNumberConvolutionalDimensions; ++DimensionIndex)
	{
		EffectiveKernelSizes[DimensionIndex] = Dilations[DimensionIndex] * (InWSizes[DimensionIndex + 2] - 1) + 1;
	}
	return EffectiveKernelSizes;
}

bool FConvBaseOperator::EstimateAndFillPads(const TArray<int64>& InEffectiveKernelSizes, const int32 InNumberDimensions, const TArray<int64>& InXSizes)
{
	const int32 NumberConvolutionalDimensions = InNumberDimensions - 2;
	// Create and fill Pads (which might be empty)
	// Explicit or no padding is used
	if (AutoPad == EAutoPad::NotSet || AutoPad == EAutoPad::Valid)
	{
		// Set Pads (if empty)
		if (Pads.IsEmpty())
		{
			Pads.Init(0, 2 * NumberConvolutionalDimensions, /*bShouldCreate64BitVersion*/true);
		}
		// If transposed conv
		if (bIsTransposed)
		{
			// Pads --> Equivalent non-transposed pads
			Pads = FPrivateConvBaseOperator::TransposePadsFromConvPads(Pads, AutoPad, InEffectiveKernelSizes);
		}
	}
	// AutoPad == Same(Lower/Upper)
	// If the padding is an odd number, the extra padding is added at the beginning (EAutoPad::SameLower) or end (EAutoPad::SameUpper)
	// How to obtain the total padding? o = Floor( (i + pL + pR - InEffectiveKernelSizes) / s) + 1
	//     - The minimum value of padding is assuming the floor does not have remainder (i.e., removing the floor) --> p_min = (o-1) x s - i + InEffectiveKernelSizes, and we have s total solutions for this equation
	//     - The maximum value of padding is assuming the floor remainder is s-1 --> p_max = o s - i + InEffectiveKernelSizes - 1
	//     - The standard seems to be using p_min, so we use that one
	// output_shape[i] = Ceil(input_shape[i] / strides[i]) for each axis i, padding split between the 2 sides equally
	else if (AutoPad == EAutoPad::SameLower || AutoPad == EAutoPad::SameUpper)
	{
		TArray<int64> PadsArrayTemp;
		PadsArrayTemp.SetNumUninitialized(2 * NumberConvolutionalDimensions);
		const TArray<int64>& XOrXWithZerosSizes = (AuxiliaryTensors.IsEmpty() ? InXSizes : AuxiliaryTensors[0]->GetSizes()); // XSizes for conv and Stride = 1, XWithZerosSizes for transposed with Stride > 1
		for (int64 DimensionIndex = 2; DimensionIndex < InNumberDimensions; ++DimensionIndex)
		{
			const int64 OutputSize = (!bIsTransposed ? FMath::CeilToInt(InXSizes[DimensionIndex] / (float)Strides[DimensionIndex - 2]) // Convolution
				: InXSizes[DimensionIndex] * ConvolutionStridesIfTransposedConvolution[DimensionIndex - 2]); // Transpose convolution
			const int64 TotalPadding = (OutputSize - 1) * Strides[DimensionIndex - 2] - XOrXWithZerosSizes[DimensionIndex] + InEffectiveKernelSizes[DimensionIndex - 2];
			const int64 PaddingIndex = 2 * (DimensionIndex - 2);
			PadsArrayTemp[PaddingIndex] = TotalPadding / 2 + (AutoPad == EAutoPad::SameLower ? TotalPadding % 2 : 0);
			PadsArrayTemp[PaddingIndex + 1] = TotalPadding - PadsArrayTemp[PaddingIndex];
		}
		Pads.Move(PadsArrayTemp);
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::EstimateAndFillPads(): Unknown AutoPad = %d."), (int32)AutoPad);
		return false;
	}
	return true;
}

bool FConvBaseOperator::FillOutputSizesAndPaddedMarginsAndSanityChecks(TArray<int64>& InOutPartiallySetOutputSizes, const TArray<int64>& InEffectiveKernelSizes)
{
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	const FNeuralTensor& XOrXWithZeros = *(AuxiliaryTensors.Num() == 0 ? InputTensors[0] : AuxiliaryTensors[0]);
	const TArray<int64>& XOrXWithZerosSizes = XOrXWithZeros.GetSizes();
	const TArray<int64>& WSizes = InputTensors[1]->GetSizes();
	const int32 NumberDimensions = XOrXWithZeros.GetNumberDimensions(); // InputTensors[0] if normal convolution
	const int32 NumberConvolutionalDimensions = NumberDimensions - 2;
	// XWithZeros.GetSize(Index>1) >= Dilations*(W.GetSize(Index>1)-1) + 1
	// I.e., each dimension of input XWithZeros must be greater than each dimension of the weights W to avoid segmentation fault (given current implementation)
	for (int64 DimensionIndex = 2; DimensionIndex < NumberDimensions; ++DimensionIndex)
	{
		const int64 PaddingIndex = 2 * (DimensionIndex - 2);
		if (XOrXWithZerosSizes[DimensionIndex] + Pads[PaddingIndex] + Pads[PaddingIndex + 1] < InEffectiveKernelSizes[DimensionIndex - 2])
		{
			UE_LOG(LogNeuralNetworkInference, Warning,
				TEXT("FConvBaseOperator::FillOutputSizesAndPaddedMarginsAndSanityChecks(): XOrXWithZerosSizes[%d] >= InEffectiveKernelSizes[%d] failed (%d vs. %d)."
					" I.e., each dimension of input XOrXXWithZeros must be greater than each dimension of the effective kernel/weight size W (taking the dilation into account)."),
				DimensionIndex, DimensionIndex - 2, XOrXWithZerosSizes[DimensionIndex], InEffectiveKernelSizes[DimensionIndex - 2]);
			return false;
		}
	}
	// Set InOutPartiallySetOutputSizes based on XOrXWithZeros, W, and padding
	for (int64 DimensionIndex = 2; DimensionIndex < NumberDimensions; ++DimensionIndex)
	{
		const int64 ActualDimension = DimensionIndex - 2;
		const int64 PaddingIndex = 2 * ActualDimension;
		const int64 ExpectedOutputSize = /*Implicit floor*/((XOrXWithZerosSizes[DimensionIndex] + Pads[PaddingIndex] + Pads[PaddingIndex + 1] - InEffectiveKernelSizes[DimensionIndex - 2]) / Strides[ActualDimension]) + 1;
		const int64 OutputPad = (OutputPadding.Num() > 0 ? OutputPadding[ActualDimension] : 0);
		// For Conv a = 0, but for ConvTranspose: o' = s(i' - 1) + a + k - 2p, where a = #zeros added at the end of XOrXWithZerosSizes
		InOutPartiallySetOutputSizes[DimensionIndex] = (OutputShape.Num() > 0 ? OutputShape[ActualDimension] : ExpectedOutputSize + OutputPad);
	}
	const TArray<int64>& OutputSizes = InOutPartiallySetOutputSizes; // At this point, InOutPartiallySetOutputSizesis finally OutputSizes
	// Set OutputPaddedMargins
	// Sides limits deduction
	// - Formula: i_i = d w_i + o_i s - pL
	// - Left side: i_i >= 0? That means w_i = 0 (because 1,2,...,k are gonna give a higher result) --> 0 <= 0 + o_i s - pL --> o_i s >= pL --> i_i >= 0 if o_i >= pL/s --> To account for pL % s != 0
	//     --> i_i >= 0 if o_i >= Ceil(pL/s) >= pL/s   and   i_i might be < 0 if o_i < Ceil(pL/s)
	// - Right side: i_i <= i-1? That means w_i = k-1 --> i-1 >= d(k-1) + o_i s - pL --> o_i s <= i-1 - d(k-1) + pL --> o_i <= [i - 1 - d(k-1) + pL]/s
	//     --> i_i <= i-1 if o_i <= Floor([i - 1 - d(k-1) + pL]/s) <= [i - 1 - d(k-1) + pL]/s   and   i_i might be > i-1 if o_i > Floor([i - 1 + pL - d(k-1)]/s)
	// Particular cases:
	// - If Pads[i] = 0 --> OutputPaddedMargins = [0, XSize]
	// - If Strides[i] = 1 --> OutputPaddedMargins = [PadsLeft[i], OutputSize-PadsRight[i]]
	OutputPaddedMargins.SetNumUninitialized(Pads.Num());
	for (int64 DimensionIndex = 0; DimensionIndex < NumberConvolutionalDimensions; ++DimensionIndex)
	{
		const int64 PaddingIndex = 2 * DimensionIndex;
		OutputPaddedMargins[PaddingIndex] = FMath::CeilToInt(Pads[PaddingIndex] / (float)Strides[DimensionIndex]);
		const int64 OutputPaddedMarginsRight = 1 + FMath::FloorToInt( // +1 because of 0 index
			(XOrXWithZerosSizes[DimensionIndex + 2] - 1 - Dilations[DimensionIndex] * (WSizes[DimensionIndex + 2] - 1) + Pads[PaddingIndex]) / (float)Strides[DimensionIndex]);
		// Problem: Because we are using floor and ceil, it might happen that OutputPaddedMargins[PaddingIndex + 1] < OutputPaddedMargins[PaddingIndex], which
		// would result in overriding the same value twice, generating wrong results!
		// Fix: OutputPaddedMargins[PaddingIndex + 1] = Max(both OutputPaddedMargins) to avoid applying the convolution twice over the same pixel. This way,
		// CPU_CONVOLUTION_XD() is not executed and CPU_CONVOLUTION_XD_PADDED() processes all pixels (because they would all need padding).
		OutputPaddedMargins[PaddingIndex + 1] = FMath::Max(OutputPaddedMargins[PaddingIndex], OutputPaddedMarginsRight);
		// Self sanity checks
		// If s==1 || Pads[i] == 0 --> OutputPaddedMargins = [0, OutputSizes]
		const int64 OutputShapeIndex = (OutputShape.Num() > 0 ? OutputShape[DimensionIndex] : 0);
		if (OutputShapeIndex == 0 && (Strides[DimensionIndex] == 1 || (Pads[PaddingIndex] == 0 && Pads[PaddingIndex + 1] == 0)))
		{
			const int64 OutputPad = (OutputPadding.Num() > 0 ? OutputPadding[DimensionIndex] : 0);
			if (OutputPaddedMargins[PaddingIndex] != Pads[PaddingIndex] || OutputPaddedMarginsRight != OutputSizes[DimensionIndex + 2] - Pads[PaddingIndex + 1] - OutputPad)
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FConvBaseOperator::FillOutputSizesAndPaddedMarginsAndSanityChecks(): Even though s=1 and/or Pads[i]=0, OutputPaddedMargins[%d,%d] != [Pads[%d], Output[%d]-Pads[%d]-OutputPad],"
						" i.e., [%d,%d] != [%d,%d-%d-%d], where the final OutputPaddedMarginsRight = %d."),
					PaddingIndex, PaddingIndex + 1, PaddingIndex, DimensionIndex + 2, PaddingIndex + 1,
					OutputPaddedMargins[PaddingIndex], OutputPaddedMarginsRight, Pads[PaddingIndex], OutputSizes[DimensionIndex + 2], Pads[PaddingIndex + 1], OutputPad, OutputPaddedMargins[PaddingIndex + 1]);
				return false;
			}
		}
		// If s>1 && Pads[i] > 0 --> Make sure OutputPaddedMargins[i] >= 0 && OutputPaddedMargins[i+1] < OutputSizes
		else
		{
			if (OutputPaddedMargins[PaddingIndex] < 0 || OutputPaddedMargins[PaddingIndex + 1] > OutputSizes[DimensionIndex + 2])
			{
				UE_LOG(LogNeuralNetworkInference, Warning,
					TEXT("FConvBaseOperator::FillOutputSizesAndPaddedMarginsAndSanityChecks(): OutputPaddedMargins[%d]=%d < 0 || OutputPaddedMargins[%d]=%d > OutputSizes[%d]=%d. Other info: s=%d, Pads[%d,%d+1]=[%d,%d]."),
					PaddingIndex, OutputPaddedMargins[PaddingIndex], PaddingIndex + 1, OutputPaddedMargins[PaddingIndex + 1], DimensionIndex + 2, OutputSizes[DimensionIndex + 2],
					Strides[DimensionIndex], PaddingIndex, PaddingIndex + 1, Pads[PaddingIndex], Pads[PaddingIndex + 1]);
				return false;
			}
		}
	}
	// Initialize OutputTensor
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
	OutputTensor.SetNumUninitialized(XOrXWithZeros.GetDataType(), OutputSizes);
	// Sanity check
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(GetOutputTensorsConst(), 1, 1, 3, -1))
	{
		return false;
	}
	return true;
}

#define MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_INIT() \
	/* Per batch N */ \
	for (int64 BatchIndex = 0; BatchIndex < OutputSizes[0]; ++BatchIndex) \
	{ \
		/* Per output channel M */ \
		for (int64 OutputChannelIndex = 0; OutputChannelIndex < OutputSizes[1]; ++OutputChannelIndex) \
		{ \
			const uint64 YPtrBatchChannelIndex = BatchIndex * OutputBatchVolume + OutputChannelIndex * OutputImageArea; \
			/* Per input channel C */ \
			for (int64 InputChannelIndex = 0; InputChannelIndex < !bIsTransposed * WSizes[1] + bIsTransposed * WSizes[0] / Group; ++InputChannelIndex) /* WSizes[1] and not XOrXWithZerosSizes[1] for grouped convolution */ \
			{ \
				const uint64 GroupedInputChannelIndex = InputChannelIndex + bIsTransposed * (OutputChannelIndex / MIntoGroup * CIntoGroup); \
				const uint64 WBatchChannelIndex = !bIsTransposed * (OutputChannelIndex * WBatchVolume + InputChannelIndex * WImageArea) + bIsTransposed * (InputChannelIndex * WBatchVolume + OutputChannelIndex * WImageArea); \
				const uint64 XBatchChannelIndex = BatchIndex * XOrXWithZerosBatchVolume + GroupedInputChannelIndex * XOrXWithZerosImageArea \
					+  /* For grouped convolution */ !bIsTransposed * (OutputChannelIndex/MIntoGroup * WSizes[1]);
#define CPU_CONVOLUTION_FOR_LOOP_X1_INIT(ForX1Begin, ForX1End) \
				/* Convolution - D1 */ \
				for (int64 OutputD1Index = ForX1Begin; OutputD1Index < ForX1End; ++OutputD1Index) /* OutputD1Index = 0 : OutputSizes[2] if no padding */ \
				{
#define CPU_CONVOLUTION_FOR_LOOP_X2_INIT(ForX2Begin, ForX2End) \
					/* Convolution - D2 */ \
					for (int64 OutputD2Index = ForX2Begin; OutputD2Index < ForX2End; ++OutputD2Index) /* OutputD2Index = 0 : OutputSizes[3] if no padding */ \
					{
#define CPU_CONVOLUTION_FOR_LOOP_X3_INIT(ForX3Begin, ForX3End) \
						/* Convolution - D3 */ \
						for (int64 OutputD3Index = ForX3Begin; OutputD3Index < ForX3End; ++OutputD3Index) /* OutputD3Index = 0 : OutputSizes[4] if no padding */ \
						{
#define CPU_CONVOLUTION_FOR_LOOP_W1_INIT() \
							/* Convolution - D1k */ \
							for (int64 W1Index = 0; W1Index < WSizes[2]; ++W1Index) \
							{ \
								const int64 X1Index = (Dilations[0] * W1Index + OutputD1Index * Strides[0] - Pads[0]); /* Pads[i] = 0 if no padding */
#define CPU_CONVOLUTION_FOR_LOOP_W2_INIT() \
								/* Convolution - D2k */ \
								for (int64 W2Index = 0; W2Index < WSizes[3]; ++W2Index) \
								{ \
									const int64 X2Index = (Dilations[1] * W2Index + OutputD2Index * Strides[1] - Pads[2]); /* Pads[i] = 0 if no padding */
#define CPU_CONVOLUTION_FOR_LOOP_W3_INIT() \
									/* Convolution - D3k */ \
									for (int64 W3Index = 0; W3Index < WSizes[4]; ++W3Index) \
									{ \
										const int64 X3Index = (Dilations[2] * W3Index + OutputD3Index * Strides[2] - Pads[4]); /* Pads[i] = 0 if no padding */

#define CPU_CONVOLUTION_FOR_LOOP_W3_END() \
									}
#define CPU_CONVOLUTION_FOR_LOOP_W2_END() \
								}
#define CPU_CONVOLUTION_FOR_LOOP_W1_END() \
							}
#define CPU_CONVOLUTION_FOR_LOOP_X3_END() \
						}
#define CPU_CONVOLUTION_FOR_LOOP_X2_END() \
					}
#define CPU_CONVOLUTION_FOR_LOOP_X1_END() \
				}
#define MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_END() \
			} \
		} \
	}

#define CPU_CONVOLUTION_1D_FOR_LOOPS_INIT(ForX1Begin, ForX1End) \
	CPU_CONVOLUTION_FOR_LOOP_X1_INIT(ForX1Begin, ForX1End) \
		const uint64 YValueIndex = YPtrBatchChannelIndex + OutputD1Index; \
		CPU_CONVOLUTION_FOR_LOOP_W1_INIT()

#define CPU_CONVOLUTION_1D_FOR_LOOPS_END() \
		CPU_CONVOLUTION_FOR_LOOP_W1_END() \
	CPU_CONVOLUTION_FOR_LOOP_X1_END()

#define CPU_CONVOLUTION_1D(ForX1Begin, ForX1End) \
	CPU_CONVOLUTION_1D_FOR_LOOPS_INIT(ForX1Begin, ForX1End) \
		OutY.At<float>(YValueIndex) += InW.At<float>(WBatchChannelIndex + W1Index) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + X1Index); \
	CPU_CONVOLUTION_1D_FOR_LOOPS_END()

#define CPU_CONVOLUTION_1D_PADDED(ForX1Begin, ForX1End) \
	CPU_CONVOLUTION_1D_FOR_LOOPS_INIT(ForX1Begin, ForX1End) \
		if (X1Index > -1 && X1Index < XOrXWithZerosSizes[2]) /* Always true if no padding */ \
		{ \
			OutY.At<float>(YValueIndex) += InW.At<float>(WBatchChannelIndex + W1Index) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + X1Index); \
		} \
	CPU_CONVOLUTION_1D_FOR_LOOPS_END()

#define CPU_CONVOLUTION_2D_FOR_LOOPS_INIT(ForX1Begin, ForX1End, ForX2Begin, ForX2End) \
	CPU_CONVOLUTION_FOR_LOOP_X1_INIT(ForX1Begin, ForX1End) \
		CPU_CONVOLUTION_FOR_LOOP_X2_INIT(ForX2Begin, ForX2End) \
			const uint64 YValueIndex = YPtrBatchChannelIndex + OutputD1Index * OutputSizes[3] + OutputD2Index; \
			CPU_CONVOLUTION_FOR_LOOP_W1_INIT() \
				CPU_CONVOLUTION_FOR_LOOP_W2_INIT()

#define CPU_CONVOLUTION_2D_FOR_LOOPS_END() \
					CPU_CONVOLUTION_FOR_LOOP_W2_END() \
				CPU_CONVOLUTION_FOR_LOOP_W1_END() \
		CPU_CONVOLUTION_FOR_LOOP_X2_END() \
	CPU_CONVOLUTION_FOR_LOOP_X1_END()

#define CPU_CONVOLUTION_2D(ForX1Begin, ForX1End, ForX2Begin, ForX2End) \
	CPU_CONVOLUTION_2D_FOR_LOOPS_INIT(ForX1Begin, ForX1End, ForX2Begin, ForX2End) \
		const int64 WIndex = W1Index * WSizes[3] + W2Index; \
		const int64 XIndex = X1Index * XOrXWithZerosSizes[3] + X2Index; \
		OutY.At<float>(YValueIndex) += InW.At<float>(WBatchChannelIndex + WIndex) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + XIndex); \
	CPU_CONVOLUTION_2D_FOR_LOOPS_END()

#define CPU_CONVOLUTION_2D_PADDED(ForX1Begin, ForX1End, ForX2Begin, ForX2End) \
	CPU_CONVOLUTION_2D_FOR_LOOPS_INIT(ForX1Begin, ForX1End, ForX2Begin, ForX2End) \
		if (X1Index > -1 && X1Index < XOrXWithZerosSizes[2] && X2Index > -1 && X2Index < XOrXWithZerosSizes[3]) /* Always true if no padding */ \
		{ \
			const int64 WIndex = W1Index * WSizes[3] + W2Index; \
			const int64 XIndex = X1Index * XOrXWithZerosSizes[3] + X2Index; \
			OutY.At<float>(YValueIndex) += InW.At<float>(WBatchChannelIndex + WIndex) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + XIndex); \
		} \
	CPU_CONVOLUTION_2D_FOR_LOOPS_END()

#define CPU_CONVOLUTION_3D_FOR_LOOPS_INIT(ForX1Begin, ForX1End, ForX2Begin, ForX2End, ForX3Begin, ForX3End) \
	CPU_CONVOLUTION_FOR_LOOP_X1_INIT(ForX1Begin, ForX1End) \
		CPU_CONVOLUTION_FOR_LOOP_X2_INIT(ForX2Begin, ForX2End) \
			CPU_CONVOLUTION_FOR_LOOP_X3_INIT(ForX3Begin, ForX3End) \
				const uint64 YValueIndex = YPtrBatchChannelIndex + (OutputD1Index * OutputSizes[3] + OutputD2Index) * OutputSizes[4] + OutputD3Index; \
				CPU_CONVOLUTION_FOR_LOOP_W1_INIT() \
					CPU_CONVOLUTION_FOR_LOOP_W2_INIT() \
						CPU_CONVOLUTION_FOR_LOOP_W3_INIT()

#define CPU_CONVOLUTION_3D_FOR_LOOPS_END() \
						CPU_CONVOLUTION_FOR_LOOP_W3_END() \
					CPU_CONVOLUTION_FOR_LOOP_W2_END() \
				CPU_CONVOLUTION_FOR_LOOP_W1_END() \
			CPU_CONVOLUTION_FOR_LOOP_X3_END() \
		CPU_CONVOLUTION_FOR_LOOP_X2_END() \
	CPU_CONVOLUTION_FOR_LOOP_X1_END()

#define CPU_CONVOLUTION_3D(ForX1Begin, ForX1End, ForX2Begin, ForX2End, ForX3Begin, ForX3End) \
	CPU_CONVOLUTION_3D_FOR_LOOPS_INIT(ForX1Begin, ForX1End, ForX2Begin, ForX2End, ForX3Begin, ForX3End) \
		const int64 WIndex = (W1Index * WSizes[3] + W2Index) * WSizes[4] + W3Index; \
		const int64 XIndex = (X1Index * XOrXWithZerosSizes[3] + X2Index) * XOrXWithZerosSizes[4] + X3Index; /* Pads[i] = 0 if no padding */ \
		OutY.At<float>(YValueIndex) += InW.At<float>(WBatchChannelIndex + WIndex) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + XIndex); \
	CPU_CONVOLUTION_3D_FOR_LOOPS_END()

#define CPU_CONVOLUTION_3D_PADDED(ForX1Begin, ForX1End, ForX2Begin, ForX2End, ForX3Begin, ForX3End) \
	CPU_CONVOLUTION_3D_FOR_LOOPS_INIT(ForX1Begin, ForX1End, ForX2Begin, ForX2End, ForX3Begin, ForX3End) \
		if (X1Index > -1 && X1Index < XOrXWithZerosSizes[2] && X2Index > -1 && X2Index < XOrXWithZerosSizes[3] && X3Index > -1 && X3Index < XOrXWithZerosSizes[4]) /* Always true if no padding */ \
		{ \
			const int64 WIndex = (W1Index * WSizes[3] + W2Index) * WSizes[4] + W3Index; \
			const int64 XIndex = (X1Index * XOrXWithZerosSizes[3] + X2Index) * XOrXWithZerosSizes[4] + X3Index; /* Pads[i] = 0 if no padding */ \
			OutY.At<float>(YValueIndex) += InW.At<float>(WBatchChannelIndex + WIndex) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + XIndex); \
		} \
	CPU_CONVOLUTION_3D_FOR_LOOPS_END()

void FConvBaseOperator::MultiChannelConvolutionCPU(FNeuralTensor& OutY, const FNeuralTensor& InXOrXWithZerosSizes, const FNeuralTensor& InW)
{
	// Convection
	// XOrXWithZeros = {N, C, Hx, Wx}
	// W = {M, C/Group, Hk, Wk}
	// Y = {N, M, Hy, Wy}
	const TArray<int64>& XOrXWithZerosSizes = InXOrXWithZerosSizes.GetSizes();
	const TArray<int64>& WSizes = InW.GetSizes();
	const TArray<int64>& OutputSizes = OutY.GetSizes();
	const int64 WBatchVolume = InW.Num() / WSizes[0];
	const int64 WImageArea = WBatchVolume / WSizes[1];
	const int64 XOrXWithZerosBatchVolume = InXOrXWithZerosSizes.Num() / XOrXWithZerosSizes[0];
	const int64 XOrXWithZerosImageArea = XOrXWithZerosBatchVolume / XOrXWithZerosSizes[1];
	const int64 OutputBatchVolume = OutY.Num() / OutputSizes[0];
	const int64 OutputImageArea = OutputBatchVolume / OutputSizes[1];
	const int32 NumberDimensions = OutY.GetNumberDimensions();
	const int64 MIntoGroup = (!bIsTransposed ? WSizes[0] / Group : WSizes[1]);
	const int64 CIntoGroup = WSizes[0] / Group;
	// 1D convolution
	// We make 1D, 2D, and 3D simple and efficient enough (there will be cache misses and lots of if statements for the padded area)
	// 1. A for loop for the middle part, which does not require any padding and will have almost no cache misses
	// 2. Different for loops with if's for all the sides (top, bottom, for 2D: left without top/bottom, right without top/bottom, etc.), which require padding
	if (NumberDimensions == 3)
	{
		// Per batch N, output channel M, input channel C
		MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_INIT()
			CPU_CONVOLUTION_1D_PADDED(	/*ForX1Begin*/0,						/*ForX1End*/OutputPaddedMargins[0])	// 1D convolution - Left padding
			CPU_CONVOLUTION_1D(			/*ForX1Begin*/OutputPaddedMargins[0],	/*ForX1End*/OutputPaddedMargins[1])	// 1D convolution
			CPU_CONVOLUTION_1D_PADDED(	/*ForX1Begin*/OutputPaddedMargins[1],	/*ForX1End*/OutputSizes[2])			// 1D convolution - Right padding
			//CPU_CONVOLUTION_1D_PADDED(	/*ForX1Begin*/0,						/*ForX1End*/OutputSizes[2])			// Less efficient equivalent - Full 1D convolution
		MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_END()
	}
	// >= 2D convolution
	else
	{
		// 2D convolution - Same than 1D convolution but adding the 2nd dimensional padding
		if (NumberDimensions == 4)
		{
			// Per batch N, output channel M, input channel C
			MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_INIT()
				// 2D convolution - Top padding
				CPU_CONVOLUTION_2D_PADDED(/*D1*/0, OutputPaddedMargins[0],						/*D2*/0, OutputSizes[3]);
				// 2D convolution - Left padding (without top/bottom)
				CPU_CONVOLUTION_2D_PADDED(/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/0, OutputPaddedMargins[2]);
				// 2D convolution - Center (not padded part)
				CPU_CONVOLUTION_2D		 (/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/OutputPaddedMargins[2], OutputPaddedMargins[3]);
				// 2D convolution - Right padding (without top/bottom)
				CPU_CONVOLUTION_2D_PADDED(/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/OutputPaddedMargins[3], OutputSizes[3]);
				// 2D convolution - Bottom padding
				CPU_CONVOLUTION_2D_PADDED(/*D1*/OutputPaddedMargins[1], OutputSizes[2],			/*D2*/0, OutputSizes[3]);
				//// Less efficient equivalent - Full 2D convolution
				//CPU_CONVOLUTION_2D_PADDED(/*D1*/0, OutputSizes[2],								/*D2*/0, OutputSizes[3]);
			MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_END()
		}
		// >= 3D convolution
		else
		{
			// 3D convolution - Same than 2D convolution but adding the 3rd dimensional padding
			if (NumberDimensions == 5)
			{
				// Per batch N, output channel M, input channel C
				MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_INIT()
					// 3D convolution - Top padding
					CPU_CONVOLUTION_3D_PADDED(/*D1*/0, OutputPaddedMargins[0],						/*D2*/0, OutputSizes[3],								/*D3*/0, OutputSizes[4]);
					// 3D convolution - Left padding (without top/bottom)
					CPU_CONVOLUTION_3D_PADDED(/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/0, OutputPaddedMargins[2],						/*D3*/0, OutputSizes[4]);
					// 3D convolution - Depth padding (without top/bottom or left/right)
					CPU_CONVOLUTION_3D_PADDED(/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/OutputPaddedMargins[2], OutputPaddedMargins[3],	/*D3*/0, OutputPaddedMargins[4]);
					// 3D convolution - Center (not padded part)
					CPU_CONVOLUTION_3D		 (/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/OutputPaddedMargins[2], OutputPaddedMargins[3],	/*D3*/OutputPaddedMargins[4], OutputPaddedMargins[5]);
					// 3D convolution - Depth padding (without top/bottom or left/right)
					CPU_CONVOLUTION_3D_PADDED(/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/OutputPaddedMargins[2], OutputPaddedMargins[3],	/*D3*/OutputPaddedMargins[5], OutputSizes[4]);
					// 3D convolution - Right padding (without top/bottom)
					CPU_CONVOLUTION_3D_PADDED(/*D1*/OutputPaddedMargins[0], OutputPaddedMargins[1],	/*D2*/OutputPaddedMargins[3], OutputSizes[3],			/*D3*/0, OutputSizes[4]);
					// 3D convolution - Bottom padding
					CPU_CONVOLUTION_3D_PADDED(/*D1*/OutputPaddedMargins[1], OutputSizes[2],			/*D2*/0, OutputSizes[3],								/*D3*/0, OutputSizes[4]);
					//// Less efficient equivalent - Full 3D convolution
					//CPU_CONVOLUTION_3D_PADDED(/*D1*/0, OutputSizes[2],								/*D2*/0, OutputSizes[3],								/*D3*/0, OutputSizes[4]);
				MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_END()
			}
			// nD convolution
			// Very inefficient, it is just meant as a backup if the user do a non expected convolution (i.e., not 1D, 2D, nor 3D).
			// If performance is required, re-implement 2D/3D idea for the desired convolution size.
			else
			{
				const int32 NumberConvolutionalDimensions = NumberDimensions - 2;
				TArray<int64> OutputImageAreaIndexes;
				TArray<int64> WImageAreaIndexes;
				// Per batch N, output channel M, input channel C
				MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_INIT()
					// nD convolution - D1, D2, ..., Dn
					OutputImageAreaIndexes.Init(0, NumberConvolutionalDimensions);
					for (int64 OutputImageAreaIndex = 0; OutputImageAreaIndex < OutputImageArea; ++OutputImageAreaIndex)
					{
						// Get output index
						const uint64 YValueIndex = YPtrBatchChannelIndex + OutputImageAreaIndex;
						float YValue = OutY.At<float>(YValueIndex);
						WImageAreaIndexes.Init(0, NumberConvolutionalDimensions);
						// nD convolution - D1k, D2k, ..., Dnk
						for (int64 WImageAreaIndex = 0; WImageAreaIndex < WImageArea; ++WImageAreaIndex)
						{
							// Get XIndex
							int64 XIndex = (Dilations[0] * WImageAreaIndexes[0] + OutputImageAreaIndexes[0] * Strides[0] - Pads[0]); // Pads[i] = 0 if no padding
							if (XIndex > -1 && XIndex < XOrXWithZerosSizes[2]) // Always true if no padding
							{
								bool bIsPaddedRegionSoItShouldSkipPixel = false;
								for (int64 XOrXWithZerosImageAreaIndex = 0; XOrXWithZerosImageAreaIndex < NumberConvolutionalDimensions - 1; ++XOrXWithZerosImageAreaIndex)
								{
									// For simplicity of the following formulas, ignoring Pads, Dilations, and Strides
									// XOrXWithZerosImageAreaIndexes[Index] = (OutputImageAreaIndexes[Index] + Dilations[Index] * WImageAreaIndexes[Index])
									// XIndex1D =   XOrXWithZerosImageAreaIndexes[0]
									// XIndex2D =   XOrXWithZerosImageAreaIndexes[0] * XOrXWithZerosSizes[3] + XOrXWithZerosImageAreaIndexes[1]
									// XIndex3D =  (XOrXWithZerosImageAreaIndexes[0] * XOrXWithZerosSizes[3] + XOrXWithZerosImageAreaIndexes[1]) * XOrXWithZerosSizes[4] + XOrXWithZerosImageAreaIndexes[2]
									// XIndex4D = ((XOrXWithZerosImageAreaIndexes[0] * XOrXWithZerosSizes[3] + XOrXWithZerosImageAreaIndexes[1]) * XOrXWithZerosSizes[4] + XOrXWithZerosImageAreaIndexes[2]) * XOrXWithZerosSizes[5] + XOrXWithZerosImageAreaIndexes[3]
									const int64 DimensionIndex = 1 + XOrXWithZerosImageAreaIndex;
									const int64 DimensionValue = (Dilations[DimensionIndex] * WImageAreaIndexes[DimensionIndex] + OutputImageAreaIndexes[DimensionIndex] * Strides[DimensionIndex]
										- Pads[2 * DimensionIndex]); // Pads[i] = 0 if no padding
									if (DimensionValue > -1 && DimensionValue < XOrXWithZerosSizes[3 + XOrXWithZerosImageAreaIndex]) // Always true if no padding
									{
										XIndex = XIndex * XOrXWithZerosSizes[3 + XOrXWithZerosImageAreaIndex] + DimensionValue;
									}
									else // Always false if no padding
									{
										bIsPaddedRegionSoItShouldSkipPixel = true;
										break;
									}
								}
								// Update output
								if (!bIsPaddedRegionSoItShouldSkipPixel) // Always true if no padding
								{
									YValue += InW.At<float>(WBatchChannelIndex + WImageAreaIndex) * InXOrXWithZerosSizes.At<float>(XBatchChannelIndex + XIndex);
								}
							}
							// Update WeightImageAreaIndexes
							FPrivateConvBaseOperator::NDTensorIndexesPlus1(WImageAreaIndexes, WSizes);
						}
						// Update OutputImageAreaIndexes TArray
						FPrivateConvBaseOperator::NDTensorIndexesPlus1(OutputImageAreaIndexes, OutputSizes);
						// Update OutY
						OutY.At<float>(YValueIndex) = YValue;
					}
				MULTICHANNEL_CPU_CONVOLUTION_FOR_LOOPS_END()
			}
		}
	}
}
