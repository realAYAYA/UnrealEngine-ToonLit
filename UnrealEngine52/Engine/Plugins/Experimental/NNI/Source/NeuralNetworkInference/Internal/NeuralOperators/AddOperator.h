// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/MultidirectionalBroadcastOperator.h"

class NEURALNETWORKINFERENCE_API FAddOperator : public IMultidirectionalBroadcastOperator
{
public:
	FAddOperator(const TSet<uint32>& InPotentialInlinedTensors);

	virtual ~FAddOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FAddOperator inlined and templated functions
 *****************************************************************************/

void FAddOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValueA, const float InValueB) { return InValueA + InValueB; });
}
