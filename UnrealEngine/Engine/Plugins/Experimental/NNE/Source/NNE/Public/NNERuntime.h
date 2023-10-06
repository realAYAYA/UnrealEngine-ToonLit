// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "NNERuntime.generated.h"

class ITargetPlatform;

UINTERFACE()
class NNE_API UNNERuntime : public UInterface
{
	GENERATED_BODY()
};

/**
 * The base interface of a neural network runtime. It is returned by UE::NNE::GetAllRuntimes() giving access to all available runtimes.
 *
 * This interface is mainly used internally by UNNEModelData to cook a model given in a file format (e.g. .onnx) into runtime specific model data.
 * The model data is then stored inside UNNEModelData which then can be used in game to create an inferable model.
 * See INNERuntimeCPU and INNERuntimeRDG on how to create a model given a UNNEModelData asset.
 */
class NNE_API INNERuntime
{
	GENERATED_BODY()

public:

	/**
	 * Get the name of the runtime.
	 *
	 * @return The name of the runtime.
	 */
	virtual FString GetRuntimeName() const = 0;
	
	/**
	 * Check if the runtime is supported on a specific platform.
	 *
	 * This function is used internally in the cooking process of UNNEModelData.
	 *
	 * @param TargetPlatform Interface identifying the target platforms.
	 * @return True if the platform is supported, false otherwise.
	 */
	virtual bool IsPlatformSupported(const ITargetPlatform* TargetPlatform) const = 0;

	/**
	 * Check if the runtime is able to create model data given some file data representing a neural network.
	 *
	 * @param FileType The type of file inside FileData. Corresponds to the file extension (e.g. 'onnx').
	 * @param FileData The raw binary file of a neural network model.
	 * @param FileId The unique identifier representing FileData.
	 * @param TargetPlatform The Interface identifying the target platform for which the data needs to be created. A null pointer indicates the currently compiled/running platform.
	 * @return True if the runtime is able to create model data, false otherwise.
	 */
	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const = 0;

	/**
	 * Create model data given some raw file data.
	 *
	 * @param FileType The type of file inside FileData. Corresponds to the file extension (e.g. 'onnx').
	 * @param FileData The raw binary file of a neural network model.
	 * @param FileId The unique identifier representing FileData.
	 * @param TargetPlatform The Interface identifying the target platform for which the data needs to be created. A null pointer indicates the currently compiled/running platform.
	 * @return Data representing the runtime specific representation of the model to be stored by UNNEModelData on success or an empty array otherwise.
	 */
	virtual TArray<uint8> CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) = 0;

	/**
	 * Get an id uniquely identifying the model data. This is used by UNNEModelData to create a FCacheKey to get and add the model data from and to a DDC.
	 *
	 * @param FileType The type of file inside FileData. Corresponds to the file extension (e.g. 'onnx').
	 * @param FileData The raw binary file of a neural network model.
	 * @param FileId The unique identifier representing FileData.
	 * @param TargetPlatform The Interface identifying the target platform for which the data is produced or by which it will be consumed. A null pointer indicates the currently compiled/running platform.
	 * @return A string that identifies the model data uniquely, e.g. by containing the FileId, model data version and target platform display name.
	 */
	virtual FString GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) = 0;
};