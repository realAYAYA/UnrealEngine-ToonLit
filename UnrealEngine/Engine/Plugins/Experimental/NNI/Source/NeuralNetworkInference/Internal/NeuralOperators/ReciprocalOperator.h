// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FReciprocalOperator : public IElementWiseOperator
{
public:
	FReciprocalOperator(const bool bIsInlinedTensor);

	virtual ~FReciprocalOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FReciprocalOperator inlined and templated functions
 *****************************************************************************/

void FReciprocalOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return 1.f/InValue; });
}
