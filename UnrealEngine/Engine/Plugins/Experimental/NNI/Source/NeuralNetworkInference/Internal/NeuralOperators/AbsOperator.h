// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FAbsOperator : public IElementWiseOperator
{
public:
	FAbsOperator(const bool bIsInlinedTensor);

	virtual ~FAbsOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FAbsOperator inlined and templated functions
 *****************************************************************************/

void FAbsOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Abs(InValue); });
}
