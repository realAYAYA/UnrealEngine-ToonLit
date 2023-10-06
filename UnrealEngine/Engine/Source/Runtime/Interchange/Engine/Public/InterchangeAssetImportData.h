// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeAssetImportData.generated.h"

class UInterchangePipelineBase;

UCLASS(BlueprintType, MinimalAPI)
class UInterchangeAssetImportData : public UAssetImportData
{
	GENERATED_BODY()
public:
	// Begin UObject interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	// End UObject interface


	/**
	 * Return the first filename stored in this data. The resulting filename will be absolute (ie, not relative to the asset).
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	FString ScriptGetFirstFilename() const
	{
		//TODO make sure this work at runtime
#if WITH_EDITORONLY_DATA
		return GetFirstFilename();
#else
		return FString();
#endif
	}

	/**
	 * Extract all the (resolved) filenames.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	TArray<FString> ScriptExtractFilenames() const
	{
		//TODO make sure this work at runtime
#if WITH_EDITORONLY_DATA
		return ExtractFilenames();
#else
		return TArray<FString>();
#endif
	}

	/**
	 * Extract all the filename labels.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	TArray<FString> ScriptExtractDisplayLabels() const
	{
		TArray<FString> TempDisplayLabels;
		//TODO make sure this work at runtime
#if WITH_EDITORONLY_DATA
		ExtractDisplayLabels(TempDisplayLabels);
#endif
		return TempDisplayLabels;
	}

#if WITH_EDITORONLY_DATA
#if WITH_EDITOR
	/**
	 * This function add tags to the asset registry.
	 */
	virtual void AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) override
	{
		if(const UInterchangeBaseNodeContainer* NodeContainerTmp = GetNodeContainer())
		{
			if (NodeContainerTmp->IsNodeUidValid(NodeUniqueID))
			{
				if (const UInterchangeBaseNode* Node = GetStoredNode(NodeUniqueID))
				{
					Node->AppendAssetRegistryTags(OutTags);
				}
			}
		}
		Super::AppendAssetRegistryTags(OutTags);
	}
#endif
#endif

	/** On a level import, set to the UInterchangeSceneImportAsset created during the import */
	UPROPERTY(EditAnywhere, Category = "Interchange | AssetImportData")
	FSoftObjectPath SceneImportAsset;

	/** Returns a pointer to the UInterchangeAssetImportData referred by the input object if applicable */
	static UInterchangeAssetImportData* GetFromObject(UObject* Object)
	{
		if (Object)
		{
			TArray<UObject*> SubObjects;
			GetObjectsWithOuter(Object, SubObjects);
			for (UObject* SubObject : SubObjects)
			{
				if (UInterchangeAssetImportData* AssetImportData = Cast<UInterchangeAssetImportData>(SubObject))
				{
					return AssetImportData;
				}
			}
		}

		return nullptr;
	}

	/** The Node UID pass to the factory that exist in the graph that was use to create this asset */
	UPROPERTY(VisibleAnywhere, Category = "Interchange | AssetImportData")
	FString NodeUniqueID;


	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API UInterchangeBaseNodeContainer* GetNodeContainer() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API void SetNodeContainer(UInterchangeBaseNodeContainer* InNodeContainer) const;

	/**
	* Returns Array of non-null pipelines
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API TArray<UObject*> GetPipelines() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API int32 GetNumberOfPipelines() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API void SetPipelines(const TArray<UObject*>& InPipelines) const;


	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API const UInterchangeBaseNode* GetStoredNode(const FString& InNodeUniqueId) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | AssetImportData")
	INTERCHANGEENGINE_API UInterchangeFactoryBaseNode* GetStoredFactoryNode(const FString& InNodeUniqueId) const;

private:
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use GetNodeContainer/SetNodeContainer instead."))
	TObjectPtr<UInterchangeBaseNodeContainer> NodeContainer_DEPRECATED;
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use GetPipelines/SetPipelines instead."))
	TArray<TObjectPtr<UObject>> Pipelines_DEPRECATED;

	UPROPERTY(Transient)
	mutable TObjectPtr<UInterchangeBaseNodeContainer> TransientNodeContainer;

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<UObject>> TransientPipelines;

	void ProcessContainerCache() const;
	void ProcessPipelinesCache() const;
	void ProcessDeprecatedData() const;
	mutable TArray64<uint8> CachedNodeContainer;
	mutable TArray<TPair<FString, FString>> CachedPipelines; //Class, Data(serialized JSON) pair
};
