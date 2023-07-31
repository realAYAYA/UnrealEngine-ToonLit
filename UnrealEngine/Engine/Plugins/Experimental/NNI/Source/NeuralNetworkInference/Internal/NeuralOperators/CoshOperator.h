// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperators/ElementWiseOperator.h"

class NEURALNETWORKINFERENCE_API FCoshOperator : public IElementWiseOperator
{
public:
	FCoshOperator(const bool bIsInlinedTensor);

	virtual ~FCoshOperator();

	virtual void ForwardCPU() override final;
};
