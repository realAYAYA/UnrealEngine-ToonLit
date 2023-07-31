// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/FloorOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FFloorOperator structors
 *****************************************************************************/

FFloorOperator::FFloorOperator(const bool bIsInlinedTensor)
	: IElementWiseOperator(TEXT("Floor"), 13, MakeShared<EElementWiseOperator>(EElementWiseOperator::Floor), bIsInlinedTensor)
{
}

FFloorOperator::~FFloorOperator()
{
}
