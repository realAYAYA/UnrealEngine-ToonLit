// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/SubOperator.h"
#include "NeuralOperatorEnumClasses.h"



/* FSubOperator structors
 *****************************************************************************/

FSubOperator::FSubOperator(const TSet<uint32>& InPotentialInlinedTensors)
	: IMultidirectionalBroadcastOperator(TEXT("Sub"), 13, MakeShared<EMultidirectionalBroadcastOperator>(EMultidirectionalBroadcastOperator::Sub), InPotentialInlinedTensors)
{
}

FSubOperator::~FSubOperator()
{
}
