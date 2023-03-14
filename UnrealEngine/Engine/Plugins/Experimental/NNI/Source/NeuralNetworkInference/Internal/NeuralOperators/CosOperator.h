// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FCosOperator : public IElementWiseOperator
{
public:
	FCosOperator(const bool bIsInlinedTensor);

	virtual ~FCosOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FCosOperator inlined and templated functions
 *****************************************************************************/

void FCosOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Cos(InValue); });
}
