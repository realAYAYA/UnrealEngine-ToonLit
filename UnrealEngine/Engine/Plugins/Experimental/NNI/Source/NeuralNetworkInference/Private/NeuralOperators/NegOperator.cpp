// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/NegOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FNegOperator structors
 *****************************************************************************/

FNegOperator::FNegOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Neg"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Neg), bIsInlinedTensor)
{
}

FNegOperator::~FNegOperator()
{
}
