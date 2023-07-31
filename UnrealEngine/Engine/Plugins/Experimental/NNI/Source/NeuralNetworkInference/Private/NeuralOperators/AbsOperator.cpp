// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/AbsOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FAbsOperator structors
 *****************************************************************************/

FAbsOperator::FAbsOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Abs"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Abs), bIsInlinedTensor)
{
}

FAbsOperator::~FAbsOperator()
{
}
