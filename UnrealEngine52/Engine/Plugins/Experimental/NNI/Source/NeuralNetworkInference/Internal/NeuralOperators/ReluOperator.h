// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FReluOperator : public IElementWiseOperator
{
public:
	FReluOperator(const bool bIsInlinedTensor);

	virtual ~FReluOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FReluOperator inlined and templated functions
 *****************************************************************************/

void FReluOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return (InValue > 0.f ? InValue : 0.f); });
}
