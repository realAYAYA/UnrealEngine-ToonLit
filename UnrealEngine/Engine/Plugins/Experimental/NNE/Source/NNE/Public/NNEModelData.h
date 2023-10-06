// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NNEModelData.generated.h"

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

	/**
	 * Initialize the model data with a copy of the data inside Buffer.
	 *
	 * This function is called by the UNNEModelDataFactory class when importing a neural network model file.
	 *
	 * @param Type A string identifying the type of data inside this asset. Corresponds to the extension of the imported file.
	 * @param Buffer The raw binary file data of the imported model to be copied into this asset.
	 */
	void Init(const FString& Type, TConstArrayView<uint8> Buffer);

	/**
	 * Get the type of data inside FileData.
	 *
	 * The FileType identifies the type of data inside FileData and typically is the extension of the file used to create the asset.
	 *
	 * @return The FileType.
	 */
	FString GetFileType();

	/**
	 * Get read only access to FileData.
	 *
	 * The FileData contains the binary data of the file which has been used to create the asset.
	 *
	 * @return The FileData.
	 */
	TConstArrayView<uint8> GetFileData();

	/**
	 * Get the FGuid identifying the FileData.
	 *
	 * The FileId is created on import of an asset. It can be used to identify the FileData, e.g. when putting corresponding data into the DDC or caching data locally.
	 *
	 * @return The FileId.
	 */
	FGuid GetFileId();

	/**
	 * Get the cached (editor) or cooked (game) optimized model data for a given runtime.
	 *
	 * This function is used by runtimes when creating a model. In editor, the function will create the optimized model data with the passed runtime in case it has not been cached in the DCC yet. In game, the cooked data is accessed.
	 *
	 * @param RuntimeName The name of the runtime for which the data should be returned.
	 * @return The optimized and runtime specific model data or an empty view in case of a failure.
	 */
	TConstArrayView<uint8> GetModelData(const FString& RuntimeName);

	/**
	 * Implements custom serialization of this asset.
	 * @param Ar The archive to serialize from/to.
	 */
	virtual void Serialize(FArchive& Ar) override;

	/**
	 * A Guid used for asset versioning.
	 */
	const static FGuid GUID;

#if WITH_EDITORONLY_DATA
	// UObject interface
	virtual void PostInitProperties() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	// End of UObject interface

	/**
	 * Get the target runtimes this model data will be cooked for. An empty list mean all runtimes.
	 *
	 * @return The target runtimes names.
	 */
	TArrayView<const FString> GetTargetRuntimes() const { return TargetRuntimes; }

	/**
	 * Set the target runtimes this model data will be cooked for. An empty list mean all runtimes.
	 *
	 * @param RuntimeNames The target runtimes names.
	 */
	void SetTargetRuntimes(TArrayView<const FString> RuntimeNames);

	/**
	 * Importing data used for this asset.
	 */
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;

private:
	/**
	 * A list of string of the supported runtime, empty to support them all.
	 */
	TArray<FString> TargetRuntimes;
#endif // WITH_EDITORONLY_DATA

private:

	/**
	 * A string identifying the type of data inside this asset. Corresponds to the extension of the imported file.
	 */
	FString FileType;

	/**
	 * The raw binary file data of the imported model.
	 */
	TArray<uint8> FileData;

	/**
	 * A Guid that uniquely identifies this model. This is used to cache optimized models in the editor.
	 */
	FGuid FileId;

	/**
	 * The processed / optimized model data for the different runtimes.
	 */
	TMap<FString, TArray<uint8>> ModelData;

};
