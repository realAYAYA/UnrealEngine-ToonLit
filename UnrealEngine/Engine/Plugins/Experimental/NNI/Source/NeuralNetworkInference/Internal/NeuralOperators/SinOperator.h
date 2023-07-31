// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FSinOperator : public IElementWiseOperator
{
public:
	FSinOperator(const bool bIsInlinedTensor);

	virtual ~FSinOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FSinOperator inlined and templated functions
 *****************************************************************************/

void FSinOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Sin(InValue); });
}
