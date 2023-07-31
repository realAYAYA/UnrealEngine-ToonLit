// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FSqrtOperator : public IElementWiseOperator
{
public:
	FSqrtOperator(const bool bIsInlinedTensor);

	virtual ~FSqrtOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FSqrtOperator inlined and templated functions
 *****************************************************************************/

void FSqrtOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Sqrt(InValue); });
}
