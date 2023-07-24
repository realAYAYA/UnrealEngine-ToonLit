// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreRuntime.h"
#include "NNECoreTypes.h"
#include "UObject/Interface.h"

#include "NNECoreRuntimeGPU.generated.h"

class UNNEModelData;

namespace UE::NNECore
{

/**
 * The tensor binding for passing input and output to IModelGPU.
 *
 * Memory is owned by the caller. The caller must make sure the buffer is large enough and at least as large as SizeInBytes.
 */
struct NNECORE_API FTensorBindingGPU
{
	void*	Data;
	uint64	SizeInBytes;
};

/**
 * The interface of a model that can run on GPU.
 *
 * Use UE::NNECore::GetRuntime<INNERuntimeGPU>(RuntimeName) to get a runtime capable of creating GPU models.
 */
class NNECORE_API IModelGPU
{
public:

	virtual ~IModelGPU() = default;

	/**
	 * Get the input tensor descriptions as defined by the model, potentially with variable dimensions.
	 *
	 * @return An array containing a tensor descriptor for each input tensor of the model.
	 */
	virtual TConstArrayView<FTensorDesc> GetInputTensorDescs() const = 0;
	
	/**
	 * Get the output tensor descriptions as defined by the model, potentially with variable dimensions.
	 *
	 * @return An array containing a tensor descriptor for each output tensor of the model.
	 */
	virtual TConstArrayView<FTensorDesc> GetOutputTensorDescs() const = 0;

	/**
	 * Get the input shapes.
	 *
	 * SetInputTensorShapes must be called prior of running a model.
	 *
	 * @return An array of input shapes or an empty array if SetInputTensorShapes has not been called.
	 */
	virtual TConstArrayView<FTensorShape> GetInputTensorShapes() const = 0;

	/**
	 * Getters for outputs shapes if they were already resolved.
	 *
	 * Output shapes might be resolved after a call to SetInputTensorShapes if the model and runtime supports it.
	 * Otherwise they will be resolved while running the model
	 *
	 * @return An array of output shapes or an empty array if not resolved yet.
	 */
	virtual TConstArrayView<FTensorShape> GetOutputTensorShapes() const = 0;
	
	/**
	 * Prepare the model to be run with the given input shape.
	 *
	 * The call is mandatory before a model can be run.
	 * The function will run shape inference and resolve, if possible, the output shapes which can then be accessed by calling GetOutputTensorShapes().
	 * This is a potentially expensive call and should be called lazily if possible.
	 *
	 * @param InInputShapes The input shapes to prepare the model with.
	 * @return 0 on success or a non-zero number otherwise.
	 */
	virtual int SetInputTensorShapes(TConstArrayView<FTensorShape> InInputShapes) = 0;

	/**
	 * Evaluate the model synchronously.
	 *
	 * SetInputTensorShapes must be called prior to this call.
	 * This function will block the calling thread until the inference is complete.
	 * The caller owns the memory inside the bindings and must make sure that they are big enough.
	 * Clients can call this function from an async task but must make sure the memory remains valid throughout the evaluation.
	 *
	 * @param InInputTensors An array containing tensor bindings for each input tensor with caller owned memory containing the input data.
	 * @param InOutputTensors An array containing tensor bindings for each output tensor with caller owned memory big enough to contain the results on success.
	 * @return 0 on success or a non-zero number otherwise.
	 */
	virtual int RunSync(TConstArrayView<FTensorBindingGPU> InInputTensors, TConstArrayView<FTensorBindingGPU> InOutputTensors) = 0;
};

} // UE::NNECore

UINTERFACE()
class NNECORE_API UNNERuntimeGPU : public UInterface
{
	GENERATED_BODY()
};

/**
 * The interface of a neural network runtime capable of creating GPU models.
 *
 * Call UE::NNECore::GetRuntime<INNERuntimeGPU>(RuntimeName) to get a runtime implementing this interface.
 */
class NNECORE_API INNERuntimeGPU
{
	GENERATED_BODY()
	
public:

	/**
	 * Check if the runtime is able to create a model given some ModelData.
	 *
	 * @param ModelData The model data for which to create a model.
	 * @return True if the runtime is able to create the model, false otherwise.
	 */
	virtual bool CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const = 0;
	
	/**
	 * Create a model given some ModelData.
	 *
	 * The caller must make sure ModelData remains valid throughout the call.
	 * ModelData is not required anymore after the model has been created.
	 * The caller can decide to convert the result into a shared pointer if required (e.g. if the model needs to be shared with an async task for evaluation).
	 *
	 * @param ModelData The model data for which to create a model.
	 * @return A caller owned model representing the neural network created from ModelData.
	 */
	virtual TUniquePtr<UE::NNECore::IModelGPU> CreateModelGPU(TObjectPtr<UNNEModelData> ModelData) = 0;
};