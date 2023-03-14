// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SignOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FSignOperator structors
 *****************************************************************************/

FSignOperator::FSignOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Sign"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Sign), bIsInlinedTensor)
{
}

FSignOperator::~FSignOperator()
{
}
