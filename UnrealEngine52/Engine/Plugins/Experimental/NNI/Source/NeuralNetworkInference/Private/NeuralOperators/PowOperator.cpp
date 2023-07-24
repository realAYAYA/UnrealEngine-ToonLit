// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/PowOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FPowOperator structors
 *****************************************************************************/

FPowOperator::FPowOperator(const TSet<uint32>& InPotentialInlinedTensors)
	: IMultidirectionalBroadcastOperator(TEXT("Pow"), 13, MakeShared<EMultidirectionalBroadcastOperator>(EMultidirectionalBroadcastOperator::Pow), InPotentialInlinedTensors)
{
}

FPowOperator::~FPowOperator()
{
}
