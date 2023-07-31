// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FSignOperator : public IElementWiseOperator
{
public:
	FSignOperator(const bool bIsInlinedTensor);

	virtual ~FSignOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FSignOperator inlined and templated functions
 *****************************************************************************/

void FSignOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Sign(InValue); });
}
