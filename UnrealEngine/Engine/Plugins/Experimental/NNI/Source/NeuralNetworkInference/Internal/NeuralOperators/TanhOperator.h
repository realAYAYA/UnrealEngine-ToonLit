// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FTanhOperator : public IElementWiseOperator
{
public:
	FTanhOperator(const bool bIsInlinedTensor);

	virtual ~FTanhOperator();

	virtual void ForwardCPU() override final;
};
