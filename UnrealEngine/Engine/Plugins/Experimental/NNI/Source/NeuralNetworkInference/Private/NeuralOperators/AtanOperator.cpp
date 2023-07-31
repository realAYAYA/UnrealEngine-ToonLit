// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/AtanOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FAtanOperator structors
 *****************************************************************************/

FAtanOperator::FAtanOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Atan"), 7, MakeShared<EElementWiseOperator>(EElementWiseOperator::Atan), bIsInlinedTensor)
{
}

FAtanOperator::~FAtanOperator()
{
}
