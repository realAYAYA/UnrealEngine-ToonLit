// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FTanOperator : public IElementWiseOperator
{
public:
	FTanOperator(const bool bIsInlinedTensor);

	virtual ~FTanOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FTanOperator inlined and templated functions
 *****************************************************************************/

void FTanOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Tan(InValue); });
}
