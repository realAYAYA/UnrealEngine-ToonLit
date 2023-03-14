// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "AssetTagsSubsystem.generated.h"

UENUM(DisplayName="Collection Share Type", meta=(ScriptName="CollectionShareType"))
enum class ECollectionScriptingShareType : uint8
{
	/** This collection is only visible to you and is not in source control. */
	Local,
	/** This collection is only visible to you. */
	Private,
	/** This collection is visible to everyone. */
	Shared,
};

UCLASS()
class ASSETTAGS_API UAssetTagsSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Create a new collection with the given name and share type.
	 *
	 * @param Name Name to give to the collection.
	 * @param ShareType Whether the collection should be local, private, or shared?
	 *
	 * @return True if the collection was created, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool CreateCollection(const FName Name, const ECollectionScriptingShareType ShareType);
	
	/**
	 * Destroy the given collection.
	 *
	 * @param Name Name of the collection to destroy.
	 *
	 * @return True if the collection was destroyed, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool DestroyCollection(const FName Name);
	
	/**
	 * Rename the given collection.
	 *
	 * @param Name Name of the collection to rename.
	 * @param NewName Name to give to the collection.
	 *
	 * @return True if the collection was renamed, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RenameCollection(const FName Name, const FName NewName);
	
	/**
	 * Re-parent the given collection.
	 *
	 * @param Name Name of the collection to re-parent.
	 * @param NewParentName Name of the new parent collection, or None to have the collection become a root collection.
	 *
	 * @return True if the collection was renamed, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool ReparentCollection(const FName Name, const FName NewParentName);

	/**
	 * Remove all assets from the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool EmptyCollection(const FName Name);

	/**
	 * Add the given asset to the given collection.
	 * 
	 * @param Name Name of the collection to modify.
	 * @param AssetPathName Asset to add (its path name, eg) /Game/MyFolder/MyAsset.MyAsset).
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags", DisplayName="Add Asset To Collection")
	bool K2_AddAssetToCollection(const FName Name, const FSoftObjectPath& AssetPath);

	UE_DEPRECATED(5.1, "Names containing full asset paths are deprecated. Use Soft Object Path instead.")
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool AddAssetToCollection(const FName Name, const FName AssetPathName);

	/**
	 * Add the given asset to the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetData Asset to add.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool AddAssetDataToCollection(const FName Name, const FAssetData& AssetData);

	/**
	 * Add the given asset to the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetPtr Asset to add.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool AddAssetPtrToCollection(const FName Name, const UObject* AssetPtr);
	
	/**
	 * Add the given assets to the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetPathNames Assets to add (their path names, eg) /Game/MyFolder/MyAsset.MyAsset).
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags", DisplayName="Add Assets To Collection")
	bool K2_AddAssetsToCollection(const FName Name, const TArray<FSoftObjectPath>& AssetPaths);
	 
	UE_DEPRECATED(5.1, "Names containing full asset paths are deprecated. Use Soft Object Path instead.")
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool AddAssetsToCollection(const FName Name, const TArray<FName>& AssetPathNames);

	/**
	 * Add the given assets to the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetDatas Assets to add.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool AddAssetDatasToCollection(const FName Name, const TArray<FAssetData>& AssetDatas);

	/**
	 * Add the given assets to the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetPtrs Assets to add.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool AddAssetPtrsToCollection(const FName Name, const TArray<UObject*>& AssetPtrs);

	/**
	 * Remove the given asset from the given collection.
	 * 
	 * @param Name Name of the collection to modify.
	 * @param AssetPath Asset to remove (its path, eg) /Game/MyFolder/MyAsset.MyAsset).
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags", DisplayName="Remove Asset From Collection")
	bool K2_RemoveAssetFromCollection(const FName Name, const FSoftObjectPath& AssetPath);

	UE_DEPRECATED(5.1, "Names containing full asset paths are deprecated, use Soft Object Path instead.")
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RemoveAssetFromCollection(const FName Name, const FName AssetPathName);

	/**
	 * Remove the given asset from the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetData Asset to remove.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RemoveAssetDataFromCollection(const FName Name, const FAssetData& AssetData);

	/**
	 * Remove the given asset from the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetPtr Asset to remove.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RemoveAssetPtrFromCollection(const FName Name, const UObject* AssetPtr);
	
	/**
	 * Remove the given assets from the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetPathNames Assets to remove (their path names, eg) /Game/MyFolder/MyAsset.MyAsset).
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags", DisplayName="Remove Assets From Collection")
	bool K2_RemoveAssetsFromCollection(const FName Name, const TArray<FSoftObjectPath>& AssetPaths);

	UE_DEPRECATED(5.1, "Names containing full asset paths are deprecated. Use Soft Object Path instead.")
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RemoveAssetsFromCollection(const FName Name, const TArray<FName>& AssetPathNames);

	/**
	 * Remove the given assets from the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetDatas Assets to remove.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RemoveAssetDatasFromCollection(const FName Name, const TArray<FAssetData>& AssetDatas);

	/**
	 * Remove the given assets from the given collection.
	 *
	 * @param Name Name of the collection to modify.
	 * @param AssetPtrs Assets to remove.
	 *
	 * @return True if the collection was modified, false otherwise (see the output log for details on error).
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool RemoveAssetPtrsFromCollection(const FName Name, const TArray<UObject*>& AssetPtrs);
#endif 	// WITH_EDITOR

	/**
	 * Check whether the given collection exists.
	 *
	 * @param Name Name of the collection to test.
	 *
	 * @return True if the collection exists, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	bool CollectionExists(const FName Name);

	/**
	 * Get the names of all available collections.
	 *
	 * @return Names of all available collections.
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	TArray<FName> GetCollections();

	/**
	 * Get the assets in the given collection.
	 * 
	 * @param Name Name of the collection to test.
	 *
	 * @return Assets in the given collection.
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	TArray<FAssetData> GetAssetsInCollection(const FName Name);

	/**
	 * Get the names of the collections that contain the given asset.
	 * 
	 * @param AssetPathName Asset to test (its path name, eg) /Game/MyFolder/MyAsset.MyAsset).
	 *
	 * @return Names of the collections that contain the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetTags", DisplayName="Get Collections Containing Asset")
	TArray<FName> K2_GetCollectionsContainingAsset(const FSoftObjectPath& AssetPath);

	UE_DEPRECATED(5.1, "Names containing full asset paths are deprecated. Use Soft Object Path instead.")
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	TArray<FName> GetCollectionsContainingAsset(const FName AssetPathName);

	/**
	 * Get the names of the collections that contain the given asset.
	 * 
	 * @param AssetData Asset to test.
	 *
	 * @return Names of the collections that contain the asset.
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	TArray<FName> GetCollectionsContainingAssetData(const FAssetData& AssetData);

	/**
	 * Get the names of the collections that contain the given asset.
	 * 
	 * @param AssetPtr Asset to test.
	 *
	 * @return Names of the collections that contain the asset.
	 */
	UFUNCTION(BlueprintCallable, Category="AssetTags")
	TArray<FName> GetCollectionsContainingAssetPtr(const UObject* AssetPtr);
};
