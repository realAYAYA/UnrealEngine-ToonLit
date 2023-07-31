// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperator.h"

class NEURALNETWORKINFERENCE_API FBatchNormalizationOperator : public FNeuralOperator
{
public:
	FBatchNormalizationOperator(const bool bIsInlinedTensor, const struct FNodeProto* const InNodeProto);

	FBatchNormalizationOperator(const bool bIsInlinedTensor, const float InEpsilon = 1e-5, const float InMomentum = 0.9f);

	virtual ~FBatchNormalizationOperator();

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override final;

	virtual void ForwardCPU() override final;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

private:
	float Epsilon;
	float Momentum;
};
