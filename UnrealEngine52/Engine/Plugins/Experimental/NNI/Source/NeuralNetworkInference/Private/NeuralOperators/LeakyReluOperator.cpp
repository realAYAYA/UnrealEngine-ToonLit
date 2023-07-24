// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/LeakyReluOperator.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralOperatorEnumClasses.h"



/* FLeakyReluOperator structors
 *****************************************************************************/

FLeakyReluOperator::FLeakyReluOperator(const bool bIsInlinedTensor, const FNodeProto* const InNodeProto)
	: FLeakyReluOperator(bIsInlinedTensor)
{
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("LeakyRelu(): Constructor not tested yet."));
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FLeakyReluOperator(): InNodeProto was a nullptr."));
		return;
	}
	if (const FAttributeProto* AlphaAttribute = FModelProto::FindElementInArray(TEXT("Alpha"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Attributes[0] = AlphaAttribute->F;
	}
}

FLeakyReluOperator::FLeakyReluOperator(const bool bIsInlinedTensor, const float InAlpha)
	: IElementWiseOperator(TEXT("LeakyRelu"), 6, MakeShared<EElementWiseOperator>(EElementWiseOperator::LeakyRelu), bIsInlinedTensor, { InAlpha })
{
}

FLeakyReluOperator::~FLeakyReluOperator()
{
}
