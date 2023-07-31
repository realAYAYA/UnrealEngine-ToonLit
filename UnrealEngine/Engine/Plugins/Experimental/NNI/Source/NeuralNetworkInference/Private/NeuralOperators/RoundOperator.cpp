// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/RoundOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FRoundOperator structors
 *****************************************************************************/

FRoundOperator::FRoundOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Round"), 11, MakeShared<EElementWiseOperator>(EElementWiseOperator::Round), bIsInlinedTensor)
{
}

FRoundOperator::~FRoundOperator()
{
}
