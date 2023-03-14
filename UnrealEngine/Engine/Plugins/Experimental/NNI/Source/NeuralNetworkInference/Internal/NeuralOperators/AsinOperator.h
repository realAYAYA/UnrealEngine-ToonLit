// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FAsinOperator : public IElementWiseOperator
{
public:
	FAsinOperator(const bool bIsInlinedTensor);

	virtual ~FAsinOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FAsinOperator inlined and templated functions
 *****************************************************************************/

void FAsinOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Asin(InValue); });
}
