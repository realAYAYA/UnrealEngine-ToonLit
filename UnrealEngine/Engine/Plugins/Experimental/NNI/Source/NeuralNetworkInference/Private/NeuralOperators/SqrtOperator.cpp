// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SqrtOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FSqrtOperator structors
 *****************************************************************************/

FSqrtOperator::FSqrtOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Sqrt"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Sqrt), bIsInlinedTensor)
{
}

FSqrtOperator::~FSqrtOperator()
{
}
