// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SinOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FSinOperator structors
 *****************************************************************************/

FSinOperator::FSinOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Sin"), 7, MakeShared<EElementWiseOperator>(EElementWiseOperator::Sin), bIsInlinedTensor)
{
}

FSinOperator::~FSinOperator()
{
}
