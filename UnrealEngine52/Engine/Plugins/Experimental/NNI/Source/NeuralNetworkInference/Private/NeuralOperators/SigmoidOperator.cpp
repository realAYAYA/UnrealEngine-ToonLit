// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SigmoidOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FSigmoidOperator structors
 *****************************************************************************/

FSigmoidOperator::FSigmoidOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Sigmoid"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Sigmoid), bIsInlinedTensor)
{
}

FSigmoidOperator::~FSigmoidOperator()
{
}
