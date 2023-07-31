// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FCeilOperator : public IElementWiseOperator
{
public:
	FCeilOperator(const bool bIsInlinedTensor);

	virtual ~FCeilOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FCeilOperator inlined and templated functions
 *****************************************************************************/

void FCeilOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::CeilToFloat(InValue); });
}
