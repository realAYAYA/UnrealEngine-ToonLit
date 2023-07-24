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

UCLASS(BlueprintType)
class INTERCHANGEENGINE_API UInterchangeAssetImportData : public UAssetImportData
{
	GENERATED_BODY()
public:

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
		if (NodeContainer->IsNodeUidValid(NodeUniqueID))
		{
			if (const UInterchangeBaseNode* Node = NodeContainer->GetNode(NodeUniqueID))
			{
				Node->AppendAssetRegistryTags(OutTags);
			}
		}
		Super::AppendAssetRegistryTags(OutTags);
	}
#endif
#endif

	/** The graph that was use to create this asset */
	UPROPERTY(VisibleAnywhere, Category = "Interchange | AssetImportData")
	TObjectPtr<UInterchangeBaseNodeContainer> NodeContainer;

	/** The Node UID pass to the factory that exist in the graph that was use to create this asset */
	UPROPERTY(VisibleAnywhere, Category = "Interchange | AssetImportData")
	FString NodeUniqueID;

	/** List of pipelines use to import an asset */
	UPROPERTY(EditAnywhere, Category = "Interchange | AssetImportData")
	TArray<TObjectPtr<UObject>> Pipelines;
};
