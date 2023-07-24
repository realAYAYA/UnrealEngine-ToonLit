// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/CoshOperator.h"
#include "NeuralOperatorEnumClasses.h"
#include <cmath>



/* FCoshOperator structors
 *****************************************************************************/

FCoshOperator::FCoshOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Cosh"), 9, MakeShared<EElementWiseOperator>(EElementWiseOperator::Cosh), bIsInlinedTensor)
{
}

FCoshOperator::~FCoshOperator()
{
}



/* FCoshOperator public functions
 *****************************************************************************/

void FCoshOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return std::cosh(InValue); });
}
