// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/MultidirectionalBroadcastOperator.h"

class NEURALNETWORKINFERENCE_API FMulOperator : public IMultidirectionalBroadcastOperator
{
public:
	FMulOperator(const TSet<uint32>& InPotentialInlinedTensors);

	virtual ~FMulOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FMulOperator inlined and templated functions
 *****************************************************************************/

void FMulOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValueA, const float InValueB) { return InValueA * InValueB; });
}
