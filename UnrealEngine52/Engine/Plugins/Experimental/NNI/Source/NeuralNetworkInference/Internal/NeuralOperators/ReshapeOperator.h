// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperator.h"

/**
 *  0 = Copy from input
 * -1 = Infer from remaining dimensions
 */
class NEURALNETWORKINFERENCE_API FReshapeOperator : public FNeuralOperator
{
public:
	FReshapeOperator(const bool bIsInlinedTensor, const struct FNodeProto* const InNodeProto);

	FReshapeOperator(const bool bIsInlinedTensor, const int64 InAllowZero = 0);

	virtual ~FReshapeOperator();

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override final;

	virtual bool SetWhetherInputTensorsNeedTransferToGPUForGPUMode() override final;

	virtual void ForwardCPU() override final;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

	FORCEINLINE virtual bool NeedsPostForward() const override final;

	virtual void PostForwardCPU() override final;

	FORCEINLINE virtual void PostForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

private:
	/**
	 * (Optional) By default, when any value in the 'shape' input is equal to zero the corresponding dimension value is copied from the input tensor dynamically.
	 * AllowZero = 1 indicates that if any value in the 'shape' input is set to zero, the zero value is honored, similar to NumPy.
	 */
	int64 AllowZero;
	
	/**
	 * If not inlined operator, this vector will be empty. If inlined, it will represent the original input size that PostForwardCPU/GPU will set.
	 */
	TArray<int64> InputSizeIfInlined;

	/**
	 * Note: It is assumed that Minus1Index == -1 up to 1 times, otherwise, this will fail.
	 */
	virtual TArray<int64> Remove0sAndMinus1sFromShape(const FNeuralTensor& InShapeTensor, const FNeuralTensor& InTensor) const final;
};



/* FNeuralOperator inlined and templated functions
 *****************************************************************************/

bool FReshapeOperator::NeedsPostForward() const
{
	return InlinedTensor > -1;
}

void FReshapeOperator::PostForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// It only needs to reshape input size
	PostForwardCPU();
}
