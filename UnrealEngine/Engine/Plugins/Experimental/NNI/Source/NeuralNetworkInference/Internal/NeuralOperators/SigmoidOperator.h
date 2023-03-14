// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FSigmoidOperator : public IElementWiseOperator
{
public:
	FSigmoidOperator(const bool bIsInlinedTensor);

	virtual ~FSigmoidOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FSigmoidOperator inlined and templated functions
 *****************************************************************************/

void FSigmoidOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return 1.f/(1.f + FMath::Exp(-InValue)); });
}
