// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperator.h"


class NEURALNETWORKINFERENCE_API FSqueezeOperator : public FNeuralOperator
{
public:
	FSqueezeOperator(const bool bIsInlinedNeuralTensor);

	virtual ~FSqueezeOperator();

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override final;

	virtual bool SetWhetherInputTensorsNeedTransferToGPUForGPUMode() override final;

	virtual void ForwardCPU() override final;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

	FORCEINLINE virtual bool NeedsPostForward() const override final;

	virtual void PostForwardCPU() override final;

	FORCEINLINE virtual void PostForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

private:
	/**
	 * If not inlined operator, this vector will be empty. If inlined, it will represent the original input size that PostForwardCPU/GPU will set.
	 */
	TArray<int64> InputSizeIfInlined;

	/**
	 * It squeezes (i.e., removes the dimensions of size 1) of the tensor dimensions specified by InAxesTensor.
	 */
	virtual TArray<int64> Remove1sFromShape(const FNeuralTensor& InTensor, const FNeuralTensor& InAxesTensor) const final;

	/**
	 * It squeezes (i.e., removes the dimensions of size 1) of all the tensor.
	 */
	virtual TArray<int64> Remove1sFromShape(const FNeuralTensor& InTensor) const final;
};



/* FNeuralOperator inlined and templated functions
 *****************************************************************************/

bool FSqueezeOperator::NeedsPostForward() const
{
	return InlinedTensor > -1;
}

void FSqueezeOperator::PostForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// It only needs to reshape input size
	PostForwardCPU();
}
