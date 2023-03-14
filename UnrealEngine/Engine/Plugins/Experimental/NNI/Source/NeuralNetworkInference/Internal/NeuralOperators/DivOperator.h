// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/MultidirectionalBroadcastOperator.h"

class NEURALNETWORKINFERENCE_API FDivOperator : public IMultidirectionalBroadcastOperator
{
public:
	FDivOperator(const TSet<uint32>& InPotentialInlinedTensors);

	virtual ~FDivOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FDivOperator inlined and templated functions
 *****************************************************************************/

void FDivOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValueA, const float InValueB) { return InValueA / InValueB; });
}
