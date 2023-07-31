// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ReluOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FReluOperator structors
 *****************************************************************************/

FReluOperator::FReluOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Relu"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Relu), bIsInlinedTensor)
{
}

FReluOperator::~FReluOperator()
{
}
