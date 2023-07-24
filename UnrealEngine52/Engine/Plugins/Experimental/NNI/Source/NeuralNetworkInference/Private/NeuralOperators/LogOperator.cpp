// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/LogOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FLogOperator structors
 *****************************************************************************/

FLogOperator::FLogOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Log"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Log), bIsInlinedTensor)
{
}

FLogOperator::~FLogOperator()
{
}
