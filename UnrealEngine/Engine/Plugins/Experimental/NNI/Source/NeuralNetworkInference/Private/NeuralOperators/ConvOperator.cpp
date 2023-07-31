// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ConvOperator.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FPrivateConvOperator auxiliary functions
 *****************************************************************************/

class FPrivateConvOperator
{
public:
	static FConvOperator NodeProtoToConvOperator(const FNodeProto* const InNodeProto);
};

FConvOperator FPrivateConvOperator::NodeProtoToConvOperator(const FNodeProto* const InNodeProto)
{
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvOperator(): InNodeProto was a nullptr."));
		return FConvOperator(FConvBaseOperator::EAutoPad::NotSet, {}, -1, {}, {}, {});
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
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FConvOperator(): Unknown EpsilonAttribute->S = %s."), *EpsilonAttribute->S);
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
	return FConvOperator(AutoPad, Dilations, Group, KernelShape, Pads, Strides);
}



/* FConvOperator structors
 *****************************************************************************/

FConvOperator::FConvOperator(const FNodeProto* const InNodeProto)
	: FConvOperator(FPrivateConvOperator::NodeProtoToConvOperator(InNodeProto))
{
}

FConvOperator::FConvOperator(const EAutoPad InAutoPad, const TArray<int64>& InDilations, const int64 InGroup, const TArray<int64>& InKernelShape, const TArray<int64>& InPads, const TArray<int64>& InStrides)
	: FConvBaseOperator(TEXT("Conv"), 11, -1, InAutoPad, InDilations, InGroup, InKernelShape, InPads, InStrides, /*bIsTransposed*/false)
{
}

FConvOperator::~FConvOperator()
{
}



/* FConvOperator protected functions
 *****************************************************************************/

bool FConvOperator::SetAndConfigureStrides(const int32 InNumberConvolutionalDimensions)
{
	// Create and fill Strides (which might be empty)
	if (Strides.Num() < 1)
	{
		Strides.Init(1, InNumberConvolutionalDimensions, /*bShouldCreate64BitVersion*/true);
	}
	return true;
}
