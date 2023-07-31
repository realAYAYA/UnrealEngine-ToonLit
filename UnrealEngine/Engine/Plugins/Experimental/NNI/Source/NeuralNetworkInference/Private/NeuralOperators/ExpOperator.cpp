// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ExpOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FExpOperator structors
 *****************************************************************************/

FExpOperator::FExpOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Exp"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Exp), bIsInlinedTensor)
{
}

FExpOperator::~FExpOperator()
{
}
