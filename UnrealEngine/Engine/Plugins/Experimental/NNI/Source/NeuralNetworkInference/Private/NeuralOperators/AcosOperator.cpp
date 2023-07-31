// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/AcosOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FAcosOperator structors
 *****************************************************************************/

FAcosOperator::FAcosOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Acos"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Acos), bIsInlinedTensor)
{
}

FAcosOperator::~FAcosOperator()
{
}
