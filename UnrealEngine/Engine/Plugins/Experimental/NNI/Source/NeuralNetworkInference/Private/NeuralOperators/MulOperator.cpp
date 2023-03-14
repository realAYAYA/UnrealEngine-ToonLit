// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/MulOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FMulOperator structors
 *****************************************************************************/

FMulOperator::FMulOperator(const TSet<uint32>& InPotentialInlinedTensors)
	: IMultidirectionalBroadcastOperator(TEXT("Mul"), 13, MakeShared<EMultidirectionalBroadcastOperator>(EMultidirectionalBroadcastOperator::Mul), InPotentialInlinedTensors)
{
}

FMulOperator::~FMulOperator()
{
}
