// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ContentBrowserAssetDataPayload.h"
#include "ContentBrowserDataSource.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/PathTree.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserItemPath.h"
#include "ContentBrowserAliasDataSource.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogContentBrowserAliasDataSource, Log, All);

class IAssetTools;
struct FContentBrowserCompiledAssetDataFilter;

/** A unique alias is a pair of SourceObjectPath:AliasPath, eg /Game/MyAsset.MyAsset:/Game/SomeFolder/MyAlias */
typedef TPair<FSoftObjectPath, FName> FContentBrowserUniqueAlias;

class CONTENTBROWSERALIASDATASOURCE_API FContentBrowserAliasItemDataPayload : public FContentBrowserAssetFileItemDataPayload
{
public:
	FContentBrowserAliasItemDataPayload(const FAssetData& InAssetData, const FContentBrowserUniqueAlias& InAlias)
		: FContentBrowserAssetFileItemDataPayload(InAssetData), Alias(InAlias)
	{
	}

	FContentBrowserUniqueAlias Alias;
};

/**
 * A companion to the ContentBrowserAssetDataSource which can display assets in folders other than their actual folder. Aliases mimic their source asset as closely as possible,
 * including editing, saving, thumbnails, and more. Some behavior is restricted such as moving or deleting an alias item.
 * 
 * Aliases can either be created automatically by tagging the asset with the value defined by AliasTagName and giving it a comma-separated list of aliases,
 * or manually managed by calling AddAlias and RemoveAlias. ReconcileAliasesForAsset is provided as a helper function to automatically update new/removed aliases for an existing asset.
 *
 */
UCLASS()
class CONTENTBROWSERALIASDATASOURCE_API UContentBrowserAliasDataSource : public UContentBrowserDataSource
{
	GENERATED_BODY()

public:
	/** The metadata tag to set for the AliasDataSource to automatically create aliases for an asset */
	static FName AliasTagName;

	// ~ Begin UContentBrowserDataSource interface
	void Initialize(const bool InAutoRegister = true);
	virtual void Shutdown() override;

	virtual void CompileFilter(const FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter) override;
	virtual void EnumerateItemsMatchingFilter(const FContentBrowserDataCompiledFilter& InFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;
	virtual void EnumerateItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;

	virtual bool DoesItemPassFilter(const FContentBrowserItemData& InItem, const FContentBrowserDataCompiledFilter& InFilter) override;

	using UContentBrowserDataSource::GetAliasesForPath;
	virtual TArray<FContentBrowserItemPath> GetAliasesForPath(const FSoftObjectPath& InInternalPath) const override;

	virtual bool GetItemAttribute(const FContentBrowserItemData& InItem, const bool InIncludeMetaData, const FName InAttributeKey, FContentBrowserItemDataAttributeValue& OutAttributeValue) override;
	virtual bool GetItemAttributes(const FContentBrowserItemData& InItem, const bool InIncludeMetaData, FContentBrowserItemDataAttributeValues& OutAttributeValues) override;
	virtual bool GetItemPhysicalPath(const FContentBrowserItemData& InItem, FString& OutDiskPath) override;
	virtual bool IsItemDirty(const FContentBrowserItemData& InItem) override;

	virtual bool CanEditItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;
	virtual bool EditItem(const FContentBrowserItemData& InItem) override;
	virtual bool BulkEditItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanPreviewItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;
	virtual bool PreviewItem(const FContentBrowserItemData& InItem) override;
	virtual bool BulkPreviewItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanSaveItem(const FContentBrowserItemData& InItem, const EContentBrowserItemSaveFlags InSaveFlags, FText* OutErrorMsg) override;
	virtual bool SaveItem(const FContentBrowserItemData& InItem, const EContentBrowserItemSaveFlags InSaveFlags) override;
	virtual bool BulkSaveItems(TArrayView<const FContentBrowserItemData> InItems, const EContentBrowserItemSaveFlags InSaveFlags) override;

	virtual bool AppendItemReference(const FContentBrowserItemData& InItem, FString& InOutStr) override;
	virtual bool UpdateThumbnail(const FContentBrowserItemData& InItem, FAssetThumbnail& InThumbnail) override;

	virtual bool TryGetCollectionId(const FContentBrowserItemData& InItem, FSoftObjectPath& OutCollectionId) override;

	// Legacy functions seem necessary for FrontendFilters to work
	virtual bool Legacy_TryGetPackagePath(const FContentBrowserItemData& InItem, FName& OutPackagePath) override;
	virtual bool Legacy_TryGetAssetData(const FContentBrowserItemData& InItem, FAssetData& OutAssetData) override;
	virtual bool Legacy_TryConvertPackagePathToVirtualPath(const FName InPackagePath, FName& OutPath) override;
	virtual bool Legacy_TryConvertAssetDataToVirtualPath(const FAssetData& InAssetData, const bool InUseFolderPaths, FName& OutPath) override;
	// ~ End UContentBrowserDataSource interface

	/** Add a list of aliases for a given asset. bInIsFromMetaData should only be true if the list of aliases came from the AliasTagName metadata. */
	void AddAliases(const FAssetData& Asset, const TArray<FName>& Aliases, bool bInIsFromMetaData = false);
	/** Add an alias for a given asset. bInIsFromMetaData should only be true if the alias came from the AliasTagName metadata. */
	void AddAlias(const FAssetData& Asset, const FName Alias, bool bInIsFromMetaData = false);
	/** Remove the given alias from the data source */
	void RemoveAlias(const FSoftObjectPath& ObjectPath, const FName Alias);
	/** Remove all aliases for the given object */
	void RemoveAliases(const FSoftObjectPath& ObjectPath);
	/** Remove all aliases for the given asset */
	void RemoveAliases(const FAssetData& Asset) { RemoveAliases(Asset.GetSoftObjectPath()); }

	UE_DEPRECATED(5.1, "FNames containing full asset paths are deprecated, use FSoftObjectPath instead")
	void RemoveAlias(const FName ObjectPath, const FName Alias);
	UE_DEPRECATED(5.1, "FNames containing full asset paths are deprecated, use FSoftObjectPath instead")
	void RemoveAliases(const FName ObjectPath);

	/** Get all aliases from metadata for the given asset, then calls AddAlias or RemoveAlias for every alias that doesn't match the stored data. */
	void ReconcileAliasesFromMetaData(const FAssetData& Asset);
	/** Calls AddAlias or RemoveAlias for every alias that doesn't match the stored data for the given asset. */
	void ReconcileAliasesForAsset(const FAssetData& Asset, const TArray<FName>& NewAliases);

protected:
	virtual void BuildRootPathVirtualTree() override;

private:
	void OnAssetAdded(const FAssetData& InAssetData);
	void OnAssetRemoved(const FAssetData& InAssetData);
	void OnAssetUpdated(const FAssetData& InAssetData);
	void OnAssetLoaded(UObject* InAsset);
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);

	/** Helper function to remove a folder from the PathTree including all parent folders that are now empty as a result of the removal */
	void RemoveFoldersRecursively(FStringView LeafFolder);
	void MakeItemModifiedUpdate(UObject* Object);

	FContentBrowserItemData CreateAssetFolderItem(const FName InFolderPath);
	FContentBrowserItemData CreateAssetFileItem(const FContentBrowserUniqueAlias& Alias);

	IAssetRegistry* AssetRegistry = nullptr;
	IAssetTools* AssetTools = nullptr;

	struct FAliasData
	{
		FAliasData() {}
		FAliasData(const FAssetData& InAssetData, const FName InPackagePath, const FName InName, const bool bInIsFromMetaData = false)
			: AssetData(InAssetData), PackagePath(InPackagePath), AliasName(InName), bIsFromMetaData(bInIsFromMetaData)
		{
			FNameBuilder PathBuilder(PackagePath);
			PathBuilder << TEXT('/');
			PathBuilder << InAssetData.AssetName;
			PackageName = FName(PathBuilder.ToView());

			ObjectPath = FSoftObjectPath(PackageName, InAssetData.AssetName, {});
		}

		/** The source asset for this alias */
		FAssetData AssetData;
		/** The folder path that contains the alias, /MyAliases */
		FName PackagePath;
		/** PackagePath/SourceAssetName, /MyAliases/SomeAsset  */
		FName PackageName;
		/** PackageName.SourceAssetName, /MyAliases/SomeAsset.SomeAsset */
		FSoftObjectPath ObjectPath;
		/** A non-unique display name for this alias */
		FName AliasName;
		/** Whether this alias was generated from package metadata or manually through the C++ interface */
		bool bIsFromMetaData = false;
	};
	bool DoesAliasPassFilter(const FAliasData& AliasData, const FContentBrowserCompiledAssetDataFilter& Filter) const;

	/** The full folder hierarchy for all alias paths */
	FPathTree PathTree;
	/** Alias data keyed by their full alias path, ie /Game/MyData/Aliases/SourceMesh */
	TMap<FContentBrowserUniqueAlias, FAliasData> AllAliases;
	/** A list of alias paths to display for each asset, ie /Game/Meshes/SourceMesh.SourceMesh */
	TMap<FSoftObjectPath, TArray<FName>> AliasesForObjectPath;
	/** A list of alias paths to display for each folder, ie /Game/MyData/Aliases */
	TMap<FName, TArray<FContentBrowserUniqueAlias>> AliasesInPackagePath;
	/** A set used for removing duplicate aliases in the same query, stored here to avoid constant reallocation */
	TSet<FSoftObjectPath> AlreadyAddedOriginalAssets;
};
