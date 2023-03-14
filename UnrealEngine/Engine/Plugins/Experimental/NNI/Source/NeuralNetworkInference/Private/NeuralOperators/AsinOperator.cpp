// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/AsinOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FAsinOperator structors
 *****************************************************************************/

FAsinOperator::FAsinOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Asin"), 7, MakeShared<EElementWiseOperator>(EElementWiseOperator::Asin), bIsInlinedTensor)
{
}

FAsinOperator::~FAsinOperator()
{
}
