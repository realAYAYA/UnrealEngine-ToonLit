// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FRoundOperator : public IElementWiseOperator
{
public:
	FRoundOperator(const bool bIsInlinedTensor);

	virtual ~FRoundOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FRoundOperator inlined and templated functions
 *****************************************************************************/

void FRoundOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::RoundToFloat(InValue); });
}
