// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralInt64ArrayUInt32Buffer.h"
#include "NeuralOperators/ConvBaseOperator.h"

class NEURALNETWORKINFERENCE_API FConvTransposeOperator : public FConvBaseOperator
{
public:
	FConvTransposeOperator(const struct FNodeProto* const InNodeProto);

	FConvTransposeOperator(const EAutoPad InAutoPad, const TArray<int64>& InDilations, const int64 InGroup, const TArray<int64>& InKernelShape,
		const TArray<int64>& InOutputPadding, const TArray<int64>& InOutputShape, const TArray<int64>& InPads, const TArray<int64>& InStrides);

	virtual ~FConvTransposeOperator();

	FORCEINLINE virtual void SetWhetherWeightTensorWasFlipped(const bool bInIsWeightTensorFlipped) final;

	virtual int32 GetNumberAuxiliaryTensors() const override final;

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override final;

	virtual void ToGPU_RenderThread() override final;

	virtual void ForwardCPU() override final;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

protected:
	virtual bool SetAndConfigureStrides(const int32 InNumberConvolutionalDimensions) override final;

	virtual bool SetAndConfigureAuxiliaryTensor() override final;

private:
	/**
	 * True if an auxiliary tensor is needed, false otherwise.
	 */
	bool bIsXWithZerosNeeded;

	/**
	 * True if the weight tensor was already transposed.
	 */
	bool bIsWeightTensorFlipped;

	/**
	 * Zeros to add between each consecutive pixel of the input tensor.
	 */
	FNeuralInt64ArrayUInt32Buffer Zeros;
	/**
	 * Size of the input tensor X (size of X needed when moving it into XWithZeros).
	 */
	FNeuralInt64ArrayUInt32Buffer XSizesForGPU;
};



/* FConvBaseOperator inlined and templated functions
 *****************************************************************************/

void FConvTransposeOperator::SetWhetherWeightTensorWasFlipped(const bool bInIsWeightTensorFlipped)
{
	bIsWeightTensorFlipped = bInIsWeightTensorFlipped;
}
