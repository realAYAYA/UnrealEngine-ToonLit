// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/ReciprocalOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FReciprocalOperator structors
 *****************************************************************************/

FReciprocalOperator::FReciprocalOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Reciprocal"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Reciprocal), bIsInlinedTensor)
{
}

FReciprocalOperator::~FReciprocalOperator()
{
}
