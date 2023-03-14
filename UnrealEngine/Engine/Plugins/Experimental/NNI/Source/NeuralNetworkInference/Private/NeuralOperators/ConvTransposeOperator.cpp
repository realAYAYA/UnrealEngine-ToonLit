// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ConvTransposeOperator.h"
#include "ConvTransposeCS.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FPrivateConvTransposeOperator auxiliary functions
 *****************************************************************************/

class FPrivateConvTransposeOperator
{
public:
	static FConvTransposeOperator NodeProtoToConvTransposeOperator(const FNodeProto* const InNodeProto);

	static bool ZerosFromConvStrides(FNeuralInt64ArrayUInt32Buffer& OutZeros, const FNeuralInt64ArrayUInt32Buffer& InStrides, const int32 InNumberConvolutionalDimensions);
};

FConvTransposeOperator FPrivateConvTransposeOperator::NodeProtoToConvTransposeOperator(const FNodeProto* const InNodeProto)
{
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator(): InNodeProto was a nullptr."));
		return FConvTransposeOperator(FConvBaseOperator::EAutoPad::NotSet, {}, -1, {}, {}, {}, {}, {});
	}
	FConvBaseOperator::EAutoPad AutoPad = FConvBaseOperator::EAutoPad::NotSet;
	if (const FAttributeProto* EpsilonAttribute = FModelProto::FindElementInArray(TEXT("AutoPad"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		if (EpsilonAttribute->S == TEXT("NOTSET"))
		{
			AutoPad = FConvBaseOperator::EAutoPad::NotSet;
		}
		else if (EpsilonAttribute->S == TEXT("SAME_UPPER"))
		{
			AutoPad = FConvBaseOperator::EAutoPad::SameUpper;
		}
		else if (EpsilonAttribute->S == TEXT("SAME_LOWER"))
		{
			AutoPad = FConvBaseOperator::EAutoPad::SameLower;
		}
		else if (EpsilonAttribute->S == TEXT("VALID"))
		{
			AutoPad = FConvBaseOperator::EAutoPad::Valid;
		}
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator(): Unknown EpsilonAttribute->S = %s."), *EpsilonAttribute->S);
		}
	}
	TArray<int64> Dilations;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("Dilations"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Dilations = MomentumAttribute->Integers;
	}
	int64 Group;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("Group"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Group = MomentumAttribute->I;
	}
	else
	{
		Group = 1;
	}
	TArray<int64> KernelShape;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("KernelShape"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		KernelShape = MomentumAttribute->Integers;
	}
	TArray<int64> Pads;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("Pads"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Pads = MomentumAttribute->Integers;
	}
	TArray<int64> Strides;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("Strides"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Strides = MomentumAttribute->Integers;
	}
	TArray<int64> Zeros;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("Zeros"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Zeros = MomentumAttribute->Integers;
	}
	TArray<int64> OutputPadding;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("OutputPadding"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		OutputPadding = MomentumAttribute->Integers;
	}
	TArray<int64> OutputShape;
	if (const FAttributeProto* MomentumAttribute = FModelProto::FindElementInArray(TEXT("OutputShape"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		OutputShape = MomentumAttribute->Integers;
	}
	return FConvTransposeOperator(AutoPad, Dilations, Group, KernelShape, OutputPadding, OutputShape, Pads, Strides);
}

bool FPrivateConvTransposeOperator::ZerosFromConvStrides(FNeuralInt64ArrayUInt32Buffer& OutZeros, const FNeuralInt64ArrayUInt32Buffer& InStrides, const int32 InNumberConvolutionalDimensions)
{
	// OutZeros from InStrides
	if (InStrides.Num() > 0)
	{
		TArray<int64> ZerosArrayTemporary;
		ZerosArrayTemporary.SetNumUninitialized(InStrides.Num());
		for (int32 DimensionIndex = 0; DimensionIndex < ZerosArrayTemporary.Num(); ++DimensionIndex)
		{
			ZerosArrayTemporary[DimensionIndex] = InStrides[DimensionIndex] - 1;
		}
		OutZeros.Move(ZerosArrayTemporary);
	}
	// OutZeros = {0, 0, ...} if no stride
	else
	{
		OutZeros.Init(0, InNumberConvolutionalDimensions, /*bShouldCreate64BitVersion*/true);
	}
	return true;
}



/* FConvTransposeOperator structors
 *****************************************************************************/

FConvTransposeOperator::FConvTransposeOperator(const FNodeProto* const InNodeProto)
	: FConvTransposeOperator(FPrivateConvTransposeOperator::NodeProtoToConvTransposeOperator(InNodeProto))
{
}

FConvTransposeOperator::FConvTransposeOperator(const FConvBaseOperator::EAutoPad InAutoPad, const TArray<int64>& InDilations, const int64 InGroup, const TArray<int64>& InKernelShape,
	const TArray<int64>& InOutputPadding, const TArray<int64>& InOutputShape, const TArray<int64>& InPads, const TArray<int64>& InStrides)
	: FConvBaseOperator(TEXT("ConvTranpose"), 11, -1, InAutoPad, InDilations, InGroup, InKernelShape, InPads, InStrides, /*bIsTransposed*/true, InOutputPadding, InOutputShape)
	, bIsXWithZerosNeeded(false)
	, bIsWeightTensorFlipped(false)
{
	// Set bIsXWithZerosNeeded to true if an auxiliary tensor is needed, false otherwise
	for (const int64 Stride : Strides)
	{
		if (Stride > 1)
		{
			bIsXWithZerosNeeded = true;
			break;
		}
	}
}

FConvTransposeOperator::~FConvTransposeOperator()
{
}



/* FConvTransposeOperator public functions
 *****************************************************************************/

int32 FConvTransposeOperator::GetNumberAuxiliaryTensors() const
{
	return (bIsXWithZerosNeeded ? 1 : 0);
}

bool FConvTransposeOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	// FConvBaseOperator sanity checks, it will directly modify bIsLoaded
	if (!FConvBaseOperator::ConfigureOutputAndInternalVariablesAndSanityChecks())
	{
		return bIsLoaded;
	}
	bIsLoaded = false;
	// bIsWeightTensorFlipped sanity check
	if (!bIsWeightTensorFlipped)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FConvTransposeOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): The weight tensor of this operator must be manually flipped with FNeuralTensor::Flip() before using this operator and SetWhetherWeightTensorWasFlipped(true) called. Dynamic weights not supported yet."));
		return bIsLoaded;
	}
	// Zeros sanity check
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	const FNeuralTensor& X = *InputTensors[0];
	const int32 NumberConvolutionalDimensions = X.GetNumberDimensions() - 2;
	if (Zeros.Num() != NumberConvolutionalDimensions)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Zeros.Num() == NumberConvolutionalDimensions failed, %d != %d."), Zeros.Num(), NumberConvolutionalDimensions);
		return bIsLoaded;
	}
	bIsLoaded = true;
	return bIsLoaded;
}

void FConvTransposeOperator::ToGPU_RenderThread()
{
	// Zeros and XSizesForGPU to GPU
	if (bIsXWithZerosNeeded)
	{
		if (Zeros.IsReadBufferValid() || XSizesForGPU.IsReadBufferValid())
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ToGPU_RenderThread():"
				" Zeros, XSizesForGPU and/or XSizesForGPU FReadBuffers were not nullptr."));
		}
		Zeros.CreateAndLoadSRVBuffer(TEXT("ConvTranpose_Zeros"));
		XSizesForGPU.CreateAndLoadSRVBuffer(TEXT("ConvTranpose_XSizesForGPU"));
	}
	FConvBaseOperator::ToGPU_RenderThread();
}

#define MULTICHANNEL_FOR_LOOPS_INIT() \
	for (int64 BatchIndex = 0; BatchIndex < XSizes[0]; ++BatchIndex) \
	{ \
		const int64 BatchOffset = BatchIndex * XSizes[1]; \
		for (int64 ChannelIndex = 0; ChannelIndex < XSizes[1]; ++ChannelIndex) \
		{ \
			const int64 BatchChannelOffset = BatchOffset + ChannelIndex; \
			const int64 XBatchChannelOffset = BatchChannelOffset * XSizes[2]; \
			const int64 XWithOffsetBatchChannelOffset = BatchChannelOffset * XWithZerosSizes[2];

#define MULTICHANNEL_FOR_LOOPS_END() \
		} \
	}

void FConvTransposeOperator::ForwardCPU()
{
	// X --> XWithZeros (i.e., AuxiliaryTensors[0])
	if (bIsXWithZerosNeeded)
	{
		FNeuralTensor& XWithZeros = *AuxiliaryTensors[0];
		const FNeuralTensor& X = *GetInputTensorsConst()[0];
		const int32 NumberDimensions = X.GetNumberDimensions();
		const TArray<int64>& XSizes = X.GetSizes();
		const TArray<int64>& XWithZerosSizes = XWithZeros.GetSizes();
		// 1D
		if (NumberDimensions == 3)
		{
			MULTICHANNEL_FOR_LOOPS_INIT()
				for (int64 D1Index = 0; D1Index < XSizes[2]; ++D1Index)
				{
					const int64 D1IndexWithZeros = D1Index * (Zeros[0] + 1);
					XWithZeros.At<float>(XWithOffsetBatchChannelOffset + D1IndexWithZeros) = X.At<float>(XBatchChannelOffset + D1Index);
				}
			MULTICHANNEL_FOR_LOOPS_END()
		}
		// 2D
		else if (NumberDimensions == 4)
		{
			MULTICHANNEL_FOR_LOOPS_INIT()
				for (int64 D1Index = 0; D1Index < XSizes[2]; ++D1Index)
				{
					const int64 D1IndexWithZeros = D1Index * (Zeros[0] + 1);
					for (int64 D2Index = 0; D2Index < XSizes[3]; ++D2Index)
					{
						const int64 D2IndexWithZeros = D2Index * (Zeros[1] + 1);
						XWithZeros.At<float>((XWithOffsetBatchChannelOffset + D1IndexWithZeros) * XWithZerosSizes[3] + D2IndexWithZeros) = X.At<float>((XBatchChannelOffset + D1Index) * XSizes[3] + D2Index);
					}
				}
			MULTICHANNEL_FOR_LOOPS_END()
		}
		// 3D
		else if (NumberDimensions == 5)
		{
			MULTICHANNEL_FOR_LOOPS_INIT()
				for (int64 D1Index = 0; D1Index < XSizes[2]; ++D1Index)
				{
					const int64 D1IndexWithZeros = D1Index * (Zeros[0] + 1);
					for (int64 D2Index = 0; D2Index < XSizes[3]; ++D2Index)
					{
						const int64 D2IndexWithZeros = D2Index * (Zeros[1] + 1);
						for (int64 D3Index = 0; D3Index < XSizes[4]; ++D3Index)
						{
							const int64 D3IndexWithZeros = D3Index * (Zeros[2] + 1);
							XWithZeros.At<float>(((XWithOffsetBatchChannelOffset + D1IndexWithZeros) * XWithZerosSizes[3] + D2IndexWithZeros) * XWithZerosSizes[4] + D3IndexWithZeros)
								= X.At<float>(((XBatchChannelOffset + D1Index) * XSizes[3] + D2Index) * XSizes[4] + D3Index);
						}
					}
				}
			MULTICHANNEL_FOR_LOOPS_END()
		}
		// 5D
		else if (NumberDimensions == 7)
		{
			MULTICHANNEL_FOR_LOOPS_INIT()
				for (int64 D1Index = 0; D1Index < XSizes[2]; ++D1Index)
				{
					const int64 D1IndexWithZeros = D1Index * (Zeros[0] + 1);
					for (int64 D2Index = 0; D2Index < XSizes[3]; ++D2Index)
					{
						const int64 D2IndexWithZeros = D2Index * (Zeros[1] + 1);
						for (int64 D3Index = 0; D3Index < XSizes[4]; ++D3Index)
						{
							const int64 D3IndexWithZeros = D3Index * (Zeros[2] + 1);
							for (int64 D4Index = 0; D4Index < XSizes[5]; ++D4Index)
							{
								const int64 D4IndexWithZeros = D4Index * (Zeros[3] + 1);
								for (int64 D5Index = 0; D5Index < XSizes[6]; ++D5Index)
								{
									const int64 D5IndexWithZeros = D5Index * (Zeros[4] + 1);
									XWithZeros.At<float>(((((XWithOffsetBatchChannelOffset + D1IndexWithZeros) * XWithZerosSizes[3] + D2IndexWithZeros) * XWithZerosSizes[4] + D3IndexWithZeros) * XWithZerosSizes[5] + D4IndexWithZeros) * XWithZerosSizes[6] + D5IndexWithZeros)
										= X.At<float>(((((XBatchChannelOffset + D1Index) * XSizes[3] + D2Index) * XSizes[4] + D3Index) * XSizes[5] + D4Index) * XSizes[6] + D5Index);
								}
							}
						}
					}
				}
			MULTICHANNEL_FOR_LOOPS_END()
		}
		// nD
		else
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("%dD transposed convolution not implemented yet."), NumberDimensions);
			return;
		}
	}
	FConvBaseOperator::ForwardCPU();
}

void FConvTransposeOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// X --> XWithZeros (i.e., AuxiliaryTensors[0])
	if (bIsXWithZerosNeeded)
	{
		// Sanity checks
		if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
			return;
		}
		const int32 NumberConvolutionalDimensions = AuxiliaryTensors[0]->GetNumberDimensions() - 2;
		if (NumberConvolutionalDimensions != 1 && NumberConvolutionalDimensions != 2 && NumberConvolutionalDimensions != 3 && NumberConvolutionalDimensions != 5)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvBaseOperator::ForwardGPU_RenderThread(): Only 1D, 2D, 3D, or 5D convolutions implemented, but not %dD."), NumberConvolutionalDimensions);
			return;
		}
		// Set parameters
		FConvTransposeCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FConvTransposeCS::FParameters>();
		// Input SRV variables - Fill ReadBuffer's the first time
		const FNeuralTensor& X = *GetInputTensorsConst()[0];
		// Input SRV variables
		Parameters->Zeros = Zeros.GetUInt32SRV();
		Parameters->XSizes = XSizesForGPU.GetUInt32SRV();
		Parameters->XWithZerosSizes = XOrXWithZerosSizesForGPU.GetUInt32SRV();
		// Input variables
		Parameters->XVolume = X.Num();
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
		Parameters->XSRV = X.GetBufferSRVRef();
		Parameters->XWithZerosUAV = AuxiliaryTensors[0]->GetBufferUAVRef();
		// Set shader
		FConvTransposeCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FConvTransposeCS::FConvMode>(ConvMode);
		TShaderMapRef<FConvTransposeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		// Run shader
		const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp(Parameters->XVolume, FConvTransposeCS::THREADGROUP_SIZE_X);
		FComputeShaderUtils::AddPass(
			*InOutGraphBuilder,
			RDG_EVENT_NAME("FConvTransposeCS - %dD convolution, Operator: %s", NumberConvolutionalDimensions, *Name),
			ComputeShader,
			Parameters,
			FIntVector(ThreadGroupCountValueX, 1, 1));
	}
	FConvBaseOperator::ForwardGPU_RenderThread(InOutGraphBuilder);
}



/* FConvTransposeOperator protected functions
 *****************************************************************************/

bool FConvTransposeOperator::SetAndConfigureStrides(const int32 InNumberConvolutionalDimensions)
{
	// Create and fill Zeros from Strides (if not empty)
	if (!FPrivateConvTransposeOperator::ZerosFromConvStrides(Zeros, Strides, InNumberConvolutionalDimensions))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureStrides(): ZerosFromConvStrides() failed."));
		return false;
	}
	// Zeros sanity checks
	if (Zeros.Num() > 0 && Zeros.Num() != InNumberConvolutionalDimensions) // Right dimensions
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureStrides(): Zeros.Num() == 0 || Zeros.Num() == InNumberConvolutionalDimensions failed (%d != 0 || %d)."),
			Zeros.Num(), InNumberConvolutionalDimensions);
		return false;
	}
	// Sanity checks
	for (const int64 Zero : Zeros) // Zeros >= 0 on each dimension
	{
		if (Zero < 0)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureStrides(): Zeros (%d) < 1 found! Zeros must be >= 1."), Zero);
			return false;
		}
		else if (!bIsXWithZerosNeeded && Zero > 0)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureStrides(): bIsXWithZerosNeeded was false but Zeros (%d) > 0 found! Only one of the 2 can be true."), Zero);
			return false;
		}
	}
	// Set ConvolutionStridesIfTransposedConvolution
	if (Strides.Num() > 0)
	{
		ConvolutionStridesIfTransposedConvolution = Strides.GetInt64();
	}
	else
	{
		ConvolutionStridesIfTransposedConvolution.Init(1, InNumberConvolutionalDimensions);
	}
	// Strides = {1, 1, ...}
	Strides.Init(1, InNumberConvolutionalDimensions, /*bShouldCreate64BitVersion*/true);
	return true;
}

bool FConvTransposeOperator::SetAndConfigureAuxiliaryTensor()
{
	// Sanity check
	if (Zeros.Num() < 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureAuxiliaryTensor(): Zeros.Num() should not be 0."));
		return false;
	}
	if (bIsXWithZerosNeeded)
	{
		const FNeuralTensor& X = *GetInputTensorsConst()[0];
		const TArray<int64>& XSizes = X.GetSizes();
		// Sanity checks
		if (XSizes.Num() < 2)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureAuxiliaryTensor(): XSizes.Num() should be at least 2, not %d."), XSizes.Num());
			return false;
		}
		else if (AuxiliaryTensors.Num() != 1)
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvTransposeOperator::SetAndConfigureAuxiliaryTensor(): AuxiliaryTensors.Num() should be 1, not %d."), AuxiliaryTensors.Num());
			return false;
		}
		// Estimate XWithZerosSizes
		TArray<int64> XWithZerosSizes = XSizes; // XWithZerosSizes[0] and [1] are the same than XSizes
		for (int32 DimensionIndex = 2; DimensionIndex < XWithZerosSizes.Num(); ++DimensionIndex)
		{
			if (XSizes[DimensionIndex] != 1) // Output dimension size = D --> XWithZerosSize = D + Zx(D-1)
			{
				XWithZerosSizes[DimensionIndex] = XSizes[DimensionIndex] + Zeros[DimensionIndex - 2] * (XSizes[DimensionIndex] - 1);
			}
			else // Output dimension size = D --> XWithZerosSize = D + Z
			{
				XWithZerosSizes[DimensionIndex] = XSizes[DimensionIndex] + Zeros[DimensionIndex - 2];
			}
		}
		// Set XWithZeros (i.e., AuxiliaryTensors[0])
		FNeuralTensor& XWithZeros = *AuxiliaryTensors[0];
		// Initialize XWithZeros
		XWithZeros.SetNumUninitialized(X.GetDataType(), XWithZerosSizes);
		XWithZeros.SetTo<float>(0.f);
		XWithZeros.SetTensorType(ENeuralTensorType::IntermediateInitialized);
		// Set XSizesForGPU
		XSizesForGPU.Copy(X.GetSizes());
	}
	return true;
}
