// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FFloorOperator : public IElementWiseOperator
{
public:
	FFloorOperator(const bool bIsInlinedTensor);

	virtual ~FFloorOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FFloorOperator inlined and templated functions
 *****************************************************************************/

void FFloorOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Floor(InValue); });
}
