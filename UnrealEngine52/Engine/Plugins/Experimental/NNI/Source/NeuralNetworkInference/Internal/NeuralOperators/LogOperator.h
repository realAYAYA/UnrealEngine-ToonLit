// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FLogOperator : public IElementWiseOperator
{
public:
	FLogOperator(const bool bIsInlinedTensor);

	virtual ~FLogOperator();

	FORCEINLINE virtual void ForwardCPU() override final;
};



/* FLogOperator inlined and templated functions
 *****************************************************************************/

void FLogOperator::ForwardCPU()
{
	ForwardCPUWithFunction([](const float InValue) { return FMath::Loge(InValue); });
}
