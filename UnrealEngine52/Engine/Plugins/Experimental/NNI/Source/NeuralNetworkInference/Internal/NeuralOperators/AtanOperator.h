// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FAtanOperator : public IElementWiseOperator
{
public:
	FAtanOperator(const bool bIsInlinedTensor);

	virtual ~FAtanOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FAtanOperator inlined and templated functions
 *****************************************************************************/

void FAtanOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Atan(InValue); });
}
