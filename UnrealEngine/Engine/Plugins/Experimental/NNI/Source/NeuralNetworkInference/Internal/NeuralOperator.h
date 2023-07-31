// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralTensor.h"

class FRDGBuilder;

/**
 * This is an auxiliary class. See UNeuralNetworkLegacy for a high-level wrapper of the whole NeuralNetworkInference plugin. The UNeuralNetworkLegacy header
 * documentation also includes some code examples.
 *
 * FNeuralOperator represents an operator (or layer) of the UNeuralNetworkLegacy.
 */
class NEURALNETWORKINFERENCE_API FNeuralOperator
{

public:
	/**
	 * @return InInlinedTensor Tensor that will be inlined, EstimateInlinedTensorFromPotentialOnes() will make no effect here.
	 * @return InPotentialInlinedTensors Potential tensors that could be inlined. EstimateInlinedTensorFromPotentialOnes() should be implemented by subclasses that use this constructor to decide the tensor that will be inlined.
	 */
	FNeuralOperator(const FString& InName, const int32 InVersion, const int32 InInlinedTensor);
	FNeuralOperator(const FString& InName, const int32 InVersion, const TSet<uint32>& InPotentialInlinedTensors);

	virtual ~FNeuralOperator();

	FORCEINLINE virtual FString GetName() const final;

	/**
	 * It returns the maximum possible number of auxiliary tensors per operator. So far, the worse case is 1 (for FConvOperator and FConvTranposeOperator).
	 * Modify 1 to a higher value if a new operator with a higher number of auxiliary tensors appear.
	 */
	FORCEINLINE static int32 GetMaximumPossibleNumberAuxiliaryTensorsPerOperator();

	/**
	 * By default, it will return 0 (meaning there is no need for auxiliary tensors).
	 * However, some operators (e.g., FConvOperator and FConvTransposeOperator) might need auxiliary tensors. If so, this function must be re-implemented and return the number of auxiliary tensors needed.
	 */
	FORCEINLINE virtual int32 GetNumberAuxiliaryTensors() const;

	/**
	 * It must be called for each operator before setting its output tensors and before calling Forward.
	 * It fills the input tensor(s) of this operator and configures it.
	 */
	virtual int32 SetInputTensorsAndGetInlinedIndex(const TArray<FNeuralTensor*>& InInputTensors) final;

	/**
	 * It must be called for operator that needs to define and use auxiliary tensors (e.g., FConvOperator and FConvTransposeOperator).
	 */
	virtual void SetAuxiliaryTensors(const TArray<FNeuralTensor*>& InAuxiliaryTensors) final;

	/**
	 * It must be called for each non-fully-inlined operator after setting its input tensors and before calling Forward.
	 */
	FORCEINLINE virtual void SetOutputTensors(const TArray<FNeuralTensor*>& InOutputTensors) final;

	/**
	 * It must be called for each operator after setting its input and output tensors and before calling Forward.
	 * It internally runs:
	 * - ConfigureOutputAndInternalVariablesAndSanityChecks(): It configures the output and auxiliary variables as well as run sanity checks to make sure input, output, and auxiliary variables all have compatible values.
	 * - SetWhetherXXXTensorsNeedTransferToGPUForGPUMode(): By default, these functions just sets SetEnableGPU(true) for all input, output, and auxiliary tensors. However, some operators (e.g.,
	 *   FReshapeOperator) override this function and only SetEnableGPU(true) to a subset of them. E.g., FReshape do not SetEnableGPU(true) the 2nd input tensor (the shape tensor).
	 *     - SetWhetherInputTensorsNeedTransferToGPUForGPUMode().
	 *     - SetWhetherOutputTensorsNeedTransferToGPUForGPUMode().
	 *     - SetWhetherAuxiliaryTensorsNeedTransferToGPUForGPUMode().
	 *
	 * @return Whether this operator was properly set and configured.
	 */
	virtual bool Configure() final;

	/**
	 * It should be implemented for those FNeuralOperator child classes that require to allocate auxiliary GPU memory.
	 * Most operators do not require this, so by default it will do nothing (not purely virtual).
	 */
	FORCEINLINE virtual void ToGPU_RenderThread();

	/**
	 * It runs the forward pass on the CPU/GPU (ForwardCPU/ForwardGPU_RenderThread). The CPU one will block the current thread until it finishes and
	 * the GPU one must be called from the render thread. By default, most child classes will not needed it.
	 */
	virtual void ForwardCPU() = 0;
	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) = 0;

	/**
	 * It runs the post forward pass on the CPU/GPU (PostForwardCPU/PostForwardGPU_RenderThread). The CPU one will block the current thread until it
	 * finishes and the GPU one must be called from the render thread. By default, most child classes will not needed it.
	 */
	FORCEINLINE virtual bool NeedsPostForward() const;
	FORCEINLINE virtual void PostForwardCPU();
	FORCEINLINE virtual void PostForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder);

	virtual FString ToString() const;

protected:
	bool bIsLoaded;
	const FString Name;
	const int32 Version;
	const TSet<uint32> PotentiallyInlinedTensors;
	int32 InlinedTensor;
	/** Only for some operators that might need them (e.g., FConvOperator and FConvTransposeOperator). */
	TArray<FNeuralTensor*> AuxiliaryTensors;

	/**
	 * 2 cases:
	 * - If PotentiallyInlinedTensors is empty, this class should not be re-implemented by the child classes.
	 * - If PotentiallyInlinedTensors is not empty, then it must be implemented by the child classes.
	 *
	 * If not empty, it decides which input tensor can be used as output (if the operator can be inlined), or -1 otherwise.
	 * There are 3 type of operators:
	 *  1. Those that cannot be inlined. This kind of operators do not need to implement EstimateInlinedTensorFromPotentialOnes() nor modify InlinedTensor.
	 *  2. Those that can be inlined. This kind of operators do not need to implement EstimateInlinedTensorFromPotentialOnes() but can instead modify InlinedTensor on construction time.
	 *  3. Those that can be inlined but only if the input tensors satisfy certain constraints (e.g., size). These must re-implement this function.
	 */
	FORCEINLINE virtual bool EstimateInlinedTensorFromPotentialOnes();

	/**
	 * See Configure() for a full explanation of these.
	 */
	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() = 0;
	virtual bool SetWhetherInputTensorsNeedTransferToGPUForGPUMode();
	virtual bool SetWhetherOutputTensorsNeedTransferToGPUForGPUMode();
	virtual bool SetWhetherAuxiliaryTensorsNeedTransferToGPUForGPUMode();

	/**
	 * GetInputTensorNoConst() is GetInputTensorsNoConst() for single output operators. It will log a warning if used with a multi-output operator.
	 */
	virtual FNeuralTensor& GetInputTensorNoConst() final;
	virtual TArray<FNeuralTensor*>& GetInputTensorsNoConst() final;

	/**
	 * GetInputTensorConst() is GetInputTensorsConst() for single output operators. It will log a warning if used with a multi-output operator.
	 */
	virtual const FNeuralTensor& GetInputTensorConst() const final;
	FORCEINLINE virtual const TArray<FNeuralTensor*>& GetInputTensorsConst() const final;

	/**
	 * GetOutputTensorNoConst() is GetOutputTensorsNoConst() for single output operators. It will log a warning if used with a multi-output operator.
	 */
	virtual FNeuralTensor& GetOutputTensorNoConst() final;
	virtual TArray<FNeuralTensor*>& GetOutputTensorsNoConst() final;

	/**
	 * GetOutputTensorConst() is GetOutputTensorsConst() for single output operators. It will log a warning if used with a multi-output operator.
	 */
	virtual const FNeuralTensor& GetOutputTensorConst() const final;
	virtual const TArray<FNeuralTensor*>& GetOutputTensorsConst() const final;

private:
	/** Const for non-inlined operators. */
	TArray<FNeuralTensor*> PrivateInputTensors;
	/** Only for non-inlined operators. */
	TArray<FNeuralTensor*> PrivateOutputTensors;
};



/* FNeuralOperator inlined and templated functions
 *****************************************************************************/

FString FNeuralOperator::GetName() const
{
	return Name;
}

int32 FNeuralOperator::GetMaximumPossibleNumberAuxiliaryTensorsPerOperator()
{
	return 1;
}

int32 FNeuralOperator::GetNumberAuxiliaryTensors() const
{
	return 0;
}

void FNeuralOperator::SetOutputTensors(const TArray<FNeuralTensor*>& InOutputTensors)
{
	PrivateOutputTensors = InOutputTensors;
}

void FNeuralOperator::ToGPU_RenderThread()
{
}

bool FNeuralOperator::NeedsPostForward() const
{
	return false;
}

void FNeuralOperator::PostForwardCPU()
{
}

void FNeuralOperator::PostForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
}

bool FNeuralOperator::EstimateInlinedTensorFromPotentialOnes()
{
	return (PotentiallyInlinedTensors.Num() == 0);
}

const TArray<FNeuralTensor*>& FNeuralOperator::GetInputTensorsConst() const
{
	return PrivateInputTensors;
}
