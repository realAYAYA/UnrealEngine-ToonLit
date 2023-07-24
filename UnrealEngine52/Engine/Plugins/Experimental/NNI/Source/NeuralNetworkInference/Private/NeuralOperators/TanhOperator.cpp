// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/TanhOperator.h"
#include "NeuralOperatorEnumClasses.h"
#include <cmath>



/* FTanhOperator structors
 *****************************************************************************/

FTanhOperator::FTanhOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Tanh"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Tanh), bIsInlinedTensor)
{
}

FTanhOperator::~FTanhOperator()
{
}



/* FTanhOperator public functions
 *****************************************************************************/

void FTanhOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return std::tanh(InValue); });
}
