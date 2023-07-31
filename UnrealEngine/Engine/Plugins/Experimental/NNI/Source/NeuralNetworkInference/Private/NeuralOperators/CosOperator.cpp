// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/CosOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FCosOperator structors
 *****************************************************************************/

FCosOperator::FCosOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Cos"), 7, MakeShared<EElementWiseOperator>(EElementWiseOperator::Cos), bIsInlinedTensor)
{
}

FCosOperator::~FCosOperator()
{
}
