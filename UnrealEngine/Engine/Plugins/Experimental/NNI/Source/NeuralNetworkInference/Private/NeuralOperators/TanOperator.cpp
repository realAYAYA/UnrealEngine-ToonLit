// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/TanOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FTanOperator structors
 *****************************************************************************/

FTanOperator::FTanOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Tan"), 7, MakeShared<EElementWiseOperator>(EElementWiseOperator::Tan), bIsInlinedTensor)
{
}

FTanOperator::~FTanOperator()
{
}
