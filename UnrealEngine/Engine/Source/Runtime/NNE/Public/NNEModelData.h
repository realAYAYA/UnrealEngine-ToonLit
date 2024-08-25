// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "Memory/SharedBuffer.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"

#include "NNEModelData.generated.h"

namespace UE::NNE
{
	/**
	 * This class implements a reference counted view on an immutable memory buffer representing model data.
	 * 
	 * It allows runtimes to reference results of GetModelData() even if they outlive UNNEModelData.
	 */
	class NNE_API FSharedModelData
	{

	public:
		/**
		 * Constructor to shared model data.
		 * 
		 * @param InData The shared buffer containing the model data. InData must be owned and the memory aligned with InMemoryAlignment.
		 * @param InMemoryAlignment The memory alignment with which InData has been aligned. A value <= 1 indicates arbitrary memory alignment.
		 */
		FSharedModelData(FSharedBuffer InData, uint32 InMemoryAlignment);

		/**
		 * Constructor to create empty data.
		 */
		FSharedModelData();

		/**
		 * Get a const array view on the shared data which is guaranteed to remain valid as long as this objects exists.
		 *
		 * @return A const array view of the shared data.
		 */
		TConstArrayView<uint8> GetView() const;

		/**
		 * Get the memory alignment with which the data has been aligned.
		 *
		 * @return Memory alignment with which the data has been aligned. A value <= 1 indicates arbitrary memory alignment.
		 */
		uint32 GetMemoryAlignment() const;

	private:

		/**
		 * The shared buffer containing the model data. Data must be aligned with MemoryAlignment.
		 */
		FSharedBuffer Data;
		
		/**
		 * The memory alignment with which Data has been aligned. A value <= 1 indicates arbitrary memory alignment.
		 */
		uint32 MemoryAlignment;
	};
}

/**
 * This class represents assets that store neural network model data.
 *
 * Neural network models typically consist of a graph of operations and corresponding parameters as e.g. weights.
 * UNNEModelData assets store such model data as imported e.g. by the UNNEModelDataFactory class.
 * An INNERuntime object retrieved by UE::NNE::GetRuntime<T>(const FString& Name) can be used to create an inferable neural network model.
 */
UCLASS(BlueprintType, Category = "NNE")
class NNE_API UNNEModelData : public UObject
{
	GENERATED_BODY()

public:

	// UObject interface
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	/**
	 * Initialize the model data with a copy of the data inside Buffer.
	 *
	 * This function is called by the UNNEModelDataFactory class when importing a neural network model file.
	 *
	 * @param Type A string identifying the type of data inside this asset. Corresponds to the extension of the imported file.
	 * @param Buffer The raw binary file data of the imported model to be copied into this asset.
	 * @param AdditionalBuffers Additional raw binary data of the model to be copied into this asset.
	 */
	void Init(const FString& Type, TConstArrayView<uint8> Buffer, const TMap<FString, TConstArrayView<uint8>>& AdditionalBuffers = TMap<FString, TConstArrayView<uint8>>());

	/**
	 * In editor: Get the target runtimes this model data will be cooked for. An empty list means all runtimes.
	 * In standalone: An empty list.
	 *
	 * @return The target runtimes names.
	 */
	TArrayView<const FString> GetTargetRuntimes() const;

	/**
	 * Set the target runtimes this model data will be cooked for. An empty list means all runtimes.
	 *
	 * @param RuntimeNames The target runtimes names.
	 */
	void SetTargetRuntimes(TArrayView<const FString> RuntimeNames);

	/**
	 * Get the type of data inside FileData.
	 *
	 * In editor: The FileType identifies the type of data inside FileData and typically is the extension of the file used to create the asset.
	 * In standalone: An empty string.
	 *
	 * @return The FileType.
	 */
	FString GetFileType() const;

	/**
	 * Get read only access to FileData.
	 *
	 * In editor: The FileData contains the binary data of the file which has been used to create the asset.
	 * In standalone: An empty array.
	 *
	 * @return The FileData.
	 */
	TConstArrayView<uint8> GetFileData() const;

	/**
	 * Get read only access to AdditionalFileData.
	 *
	 * In editor: The AdditionalFileData contains the additional binary data of the neural network model.
	 * In standalone: An empty map.
	 *
	 * @return The AdditionalFileData with a given Key if it exists and an empty view in standalone or when the key does not exist.
	 */
	TConstArrayView<uint8> GetAdditionalFileData(const FString& Key) const;

	/**
	 * Clears the FileData and the FileType.
	 *
	 * Caution, if the FileData is cleared, no more models can be created on runtimes that do not already have ModelData inside this asset.
	 */
	void ClearFileDataAndFileType();

	/**
	 * Get the FGuid identifying the FileData.
	 *
	 * The FileId is created on import of an asset. It can be used to identify the FileData, e.g. when putting corresponding data into the DDC or caching data locally.
	 * In standalone: An empty FGuid.
	 *
	 * @return The FileId.
	 */
	FGuid GetFileId() const;

	/**
	 * Get the cached (editor) or cooked (game) optimized model data for a given runtime.
	 *
	 * This function is used by runtimes when creating a model. In editor, the function will create the optimized model data with the passed runtime in case it has not been cached in the DCC yet. In game, the cooked data is accessed. The returned model data is aligned in memory as requested by the runtime.
	 *
	 * @param RuntimeName The name of the runtime for which the data should be returned.
	 * @return The optimized and runtime specific model data or an invalid TSharedPtr in case of failure.
	 */
	TSharedPtr<UE::NNE::FSharedModelData> GetModelData(const FString& RuntimeName);

	/**
	 * Clears the ModelData.
	 *
	 * Caution, if the ModelData is cleared, only runtimes that support cooking on the current platform can create new models from this asset.
	 */
	void ClearModelData();

private:
	/**
	 * A list of string of the supported runtime, empty to support them all.
	 */
	TArray<FString> TargetRuntimes;

	/**
	 * A string identifying the type of data inside this asset. Corresponds to the extension of the imported file.
	 */
	FString FileType;

	/**
	 * The raw binary file data of the imported model.
	 */
	TArray<uint8> FileData;

	/**
	 * Additional raw binary data of the imported model.
	 */
	TMap<FString, TArray<uint8>> AdditionalFileData;

	/**
	 * A Guid that uniquely identifies this model. This is used to cache optimized models in the editor.
	 */
	FGuid FileId;

	/**
	 * The processed / optimized model data for the different runtimes.
	 */
	TMap<FString, TSharedPtr<UE::NNE::FSharedModelData>> ModelData;
};
