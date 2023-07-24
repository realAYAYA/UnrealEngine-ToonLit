// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/CeilOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FCeilOperator structors
 *****************************************************************************/

FCeilOperator::FCeilOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Ceil"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Ceil), bIsInlinedTensor)
{
}

FCeilOperator::~FCeilOperator()
{
}
