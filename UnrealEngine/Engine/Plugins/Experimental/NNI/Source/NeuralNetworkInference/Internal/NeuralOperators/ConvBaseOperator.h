// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralInt64ArrayUInt32Buffer.h"
#include "NeuralOperator.h"

class NEURALNETWORKINFERENCE_API FConvBaseOperator : public FNeuralOperator
{
public:
	enum class EAutoPad : uint8
	{
		/** Explicit padding is used */
		NotSet,
		/** Auto-padding the input so that input and output match in size (if odd number, extra padding at the beginning) */
		SameUpper,
		/** Auto-padding the input so that input and output match in size (if odd number, extra padding at the end) */
		SameLower,
		/** No padding is used */
		Valid
	};

	virtual ~FConvBaseOperator();

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override;

	virtual void ToGPU_RenderThread() override;

	virtual void ForwardCPU() override;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override;

protected:
	/**
	 * Whether it is a normal or transposed convolution.
	 */
	const bool bIsTransposed;
	/**
	 * Parameters given as constructor argument / part of the child FNodeProto (might or might not be optional)
	 */
	EAutoPad AutoPad;
	FNeuralInt64ArrayUInt32Buffer Dilations;
	int64 Group;
	TArray<int64> KernelShape;
	FNeuralInt64ArrayUInt32Buffer Pads;
	FNeuralInt64ArrayUInt32Buffer Strides;
	/**
	 * Only used on ConvTransposeOperator
	 */
	TArray<int64> OutputPadding;
	TArray<int64> OutputShape;
	TArray<int64> ConvolutionStridesIfTransposedConvolution;
	/**
	 * Internally auto generated. Only for the GPU forward pass.
	 */
	FNeuralInt64ArrayUInt32Buffer OutputSizesForGPU;
	FNeuralInt64ArrayUInt32Buffer XOrXWithZerosSizesForGPU;
	FNeuralInt64ArrayUInt32Buffer WSizesForGPU;

	/**
	 * Only meant for inherited classes, which also need to fill the InName/InVersion/InInlinedTensor properties.
	 * @param InOutputPadding Only for ConvTranspose
	 */
	FConvBaseOperator(const FString& InName, const int32 InVersion, const int32 InInlinedTensor,
		const EAutoPad InAutoPad, const TArray<int64>& InDilations, const int64 InGroup, const TArray<int64>& InKernelShape, const TArray<int64>& InPads, const TArray<int64>& InStrides,
		const bool bInIsTransposed, const TArray<int64>& InOutputPadding = TArray<int64>(), const TArray<int64>& InOutputShape = TArray<int64>());

	/**
	 * It fills Strides.
	 * @return Whether the sanity checks passed.
	 */
	virtual bool SetAndConfigureStrides(const int32 InNumberConvolutionalDimensions) = 0;

	/**
	 * It sets and configures AuxiliaryTensors[0] if needed (i.e., if XOrXWithZeros is not X).
	 * @return Whether the sanity checks passed.
	 */
	FORCEINLINE virtual bool SetAndConfigureAuxiliaryTensor();

private:
	/**
	 * Internally auto generated. Only for the CPU forward pass.
	 * It defines the limits where we can guarantee the input will not go out of bounds so we can avoid the costly if 0 <= X < XSize for those pixels.
	 */
	TArray<int64> OutputPaddedMargins;

	/**
	 * Meant to be called at the very beginning of ConfigureOutputAndInternalVariablesAndSanityChecks().
	 */
	virtual bool SanityChecksForTensors(const TArray<int64>& InPartiallySetOutputSizes) const final;

	/**
	 * It fills OutputShape (if ConvTranspose and AutoPad == SameLower/Upper).
	 * @return Whether the sanity checks passed.
	 */
	virtual bool FillOutputShape(const TArray<int64>& InXSizes) final;

	/**
	 * It fills Dilations.
	 * @return Whether the sanity checks passed.
	 */
	virtual bool FillDilationsIfEmpty(const int32 InNumberConvolutionalDimensions) final;

	/**
	 * It fills and returns EffectiveKernelSizes.
	 */
	virtual TArray<int64> CreateEffectiveKernelSizes(const int32 InNumberConvolutionalDimensions, const TArray<int64>& InWSizes) final;

	/**
	 * It fills and returns Pads based on the input InPads E.g., InPads might be empty (assuming 0 padding on each dimension), the returned Pads would then be filled with NumberConvolutionalDimensions 0s.
	 * @return Whether the sanity checks passed.
	 */
	virtual bool EstimateAndFillPads(const TArray<int64>& InEffectiveKernelSizes, const int32 InNumberDimensions, const TArray<int64>& InXSizes) final;

	/**
	 * Meant to be called at the very end of ConfigureOutputAndInternalVariablesAndSanityChecks().
	 * It fills both OutputSizes and OutputPaddedMargins.
	 * @param InOutPartiallySetOutputSizes For efficiency, it will be passed by reference and modified internally.
	 */
	virtual bool FillOutputSizesAndPaddedMarginsAndSanityChecks(TArray<int64>& InOutPartiallySetOutputSizes, const TArray<int64>& InEffectiveKernelSizes) final;

	virtual void MultiChannelConvolutionCPU(FNeuralTensor& OutY, const FNeuralTensor& InXOrXWithZerosSizes, const FNeuralTensor& InW) final;
};



/* FConvBaseOperator inlined and templated functions
 *****************************************************************************/

bool FConvBaseOperator::SetAndConfigureAuxiliaryTensor()
{
	return true;
}
