// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FExpOperator : public IElementWiseOperator
{
public:
	FExpOperator(const bool bIsInlinedTensor);

	virtual ~FExpOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FExpOperator inlined and templated functions
 *****************************************************************************/

void FExpOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Exp(InValue); });
}
