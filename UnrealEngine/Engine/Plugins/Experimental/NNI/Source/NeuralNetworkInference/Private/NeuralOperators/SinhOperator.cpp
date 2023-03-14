// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SinhOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FSinhOperator structors
 *****************************************************************************/

FSinhOperator::FSinhOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Sinh"), 9, MakeShared<EElementWiseOperator>(EElementWiseOperator::Sinh), bIsInlinedTensor)
{
}

FSinhOperator::~FSinhOperator()
{
}
