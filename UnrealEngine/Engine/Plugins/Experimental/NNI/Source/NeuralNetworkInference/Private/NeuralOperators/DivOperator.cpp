// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/DivOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FDivOperator structors
 *****************************************************************************/

FDivOperator::FDivOperator(const TSet<uint32>& InPotentialInlinedTensors)
	: IMultidirectionalBroadcastOperator(TEXT("Div"), 13, MakeShared<EMultidirectionalBroadcastOperator>(EMultidirectionalBroadcastOperator::Div), InPotentialInlinedTensors)
{
}

FDivOperator::~FDivOperator()
{
}
