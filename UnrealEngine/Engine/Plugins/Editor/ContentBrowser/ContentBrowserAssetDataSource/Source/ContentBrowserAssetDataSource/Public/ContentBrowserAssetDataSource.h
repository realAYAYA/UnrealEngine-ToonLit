// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserDataSource.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/GCObject.h"
#include "Misc/NamePermissionList.h"
#include "ContentBrowserDataMenuContexts.h"
#include "ContentBrowserAssetDataPayload.h"
#include "Input/Reply.h"
#include "ContentBrowserAssetDataSource.generated.h"

class IAssetTools;
class IAssetTypeActions;
class ICollectionManager;
class UFactory;
class UToolMenu;
class FAssetFolderContextMenu;
class FAssetFileContextMenu;
struct FCollectionNameType;
class UContentBrowserToolbarMenuContext;

USTRUCT()
struct CONTENTBROWSERASSETDATASOURCE_API FContentBrowserCompiledAssetDataFilter
{
	GENERATED_BODY()

public:
	// Folder filtering
	bool bRunFolderQueryOnDemand = false;
	// On-demand filtering (always recursive on PathToScanOnDemand)
	bool bRecursivePackagePathsToInclude = false;
	bool bRecursivePackagePathsToExclude = false;
	FPathPermissionList PackagePathsToInclude;
	FPathPermissionList PackagePathsToExclude;
	FPathPermissionList PathPermissionList;
	TSet<FName> ExcludedPackagePaths;
	EContentBrowserItemAttributeFilter ItemAttributeFilter = EContentBrowserItemAttributeFilter::IncludeAll;
	FString VirtualPathToScanOnDemand;
	// Cached filtering
	TSet<FName> CachedSubPaths;

	// Asset filtering
	bool bFilterExcludesAllAssets = false;
	FARCompiledFilter InclusiveFilter;
	FARCompiledFilter ExclusiveFilter;

	// Legacy custom assets
	TArray<FAssetData> CustomSourceAssets;
};

UCLASS()
class CONTENTBROWSERASSETDATASOURCE_API UContentBrowserAssetDataSource : public UContentBrowserDataSource
{
	GENERATED_BODY()

public:
	void Initialize(const bool InAutoRegister = true);

	virtual void Shutdown() override;

	/**
	 * All of the data necessary to generate a compiled filter for folders and assets
	 */
	struct FAssetFilterInputParams
	{
		TSet<FName> InternalPaths;

		UContentBrowserDataSource* DataSource = nullptr;
		ICollectionManager* CollectionManager = nullptr;
		IAssetRegistry* AssetRegistry = nullptr;

		const FContentBrowserDataObjectFilter* ObjectFilter = nullptr;
		const FContentBrowserDataPackageFilter* PackageFilter = nullptr;
		const FContentBrowserDataClassFilter* ClassFilter = nullptr;
		const FContentBrowserDataCollectionFilter* CollectionFilter = nullptr;

		const FPathPermissionList* PathPermissionList = nullptr;
		const FPathPermissionList* ClassPermissionList = nullptr;

		FContentBrowserDataFilterList* FilterList = nullptr;
		FContentBrowserCompiledAssetDataFilter* AssetDataFilter = nullptr;

		bool bIncludeFolders = false;
		bool bIncludeFiles = false;
		bool bIncludeAssets = false;
	};

	typedef TFunctionRef<void(FName, TFunctionRef<bool(FName)>, bool)> FSubPathEnumerationFunc;
	typedef TFunctionRef<void(FARFilter&, FARCompiledFilter&)>         FCompileARFilterFunc;
	typedef TFunctionRef<FContentBrowserItemData(FName)>               FCreateFolderItemFunc;

	/**
	 * Call in CompileFilter() to populate an FAssetFilterInputParams for use in CreatePathFilter and CreateAssetFilter.
	 *
	 * @param Params The FAssetFilterInputParams struct to populate with data
	 * @param DataSource The DataSource that CompileFilter() is being called on
	 * @param InAssetRegistry A pointer to the Asset Registry to save time having to find it
	 * @param InFilter The input filter supplied to CompileFilter()
	 * @param OutCompiledFilter The output filer supplied to CompileFilter()
	 * @param CollectionManager If set, this will be used to filter objects when a collection filter is requested. If not set, and a collection filter is requested, the function will return false.
	 * 
	 * @return false if it's not possible to display folders or assets, otherwise true
	 */
	static bool PopulateAssetFilterInputParams(FAssetFilterInputParams& Params, UContentBrowserDataSource* DataSource, IAssetRegistry* InAssetRegistry, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter, ICollectionManager* CollectionManager = nullptr);
	/**
	 * Call in CompileFilter() after PopulateAssetFilterInputParams() to fill OutCompiledFilter with an FContentBrowserCompiledAssetDataFilter capable of filtering folders
	 *
	 * @param Params The params generated from PopulateAssetFilterInputParams()
	 * @param InPath The input path supplied to CompileFilter()
	 * @param InFilter The input filter supplied to CompileFilter()
	 * @param OutCompiledFilter The output filer supplied to CompileFilter()
	 * @param SubPathEnumeration A function that calls its input function on all subpaths of the given input path, optionally recursive
	 *
	 * @return false if it's not possible to display any folders, otherwise true
	 */
	static bool CreatePathFilter(FAssetFilterInputParams& Params, FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter, FSubPathEnumerationFunc SubPathEnumeration);
	/**
	 * Call in CompileFilter() after CreatePathFilter() to fill OutCompiledFilter with an FContentBrowserCompiledAssetDataFilter capable of filtering assets
	 *
	 * @param Params The params generated from PopulateAssetFilterInputParams()
	 * @param InPath The input path supplied to CompileFilter()
	 * @param InFilter The input filter supplied to CompileFilter()
	 * @param OutCompiledFilter The output filer supplied to CompileFilter()
	 * @param CreateCompiledFilter A function that generates an FARCompiledFilter from an input FARFilter
	 *
	 * @return false if it's not possible to display any assets, otherwise true
	 */
	static bool CreateAssetFilter(FAssetFilterInputParams& Params, FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter, FCompileARFilterFunc CreateCompiledFilter);
	/**
	 * Call in EnumerateItemsMatchingFilter() to generate a list of folders that match the compiled filter.
	 * It is the caller's responsibility to verify EContentBrowserItemTypeFilter::IncludeFolders is set before enumerating.
	 *
	 * @param DataSource The DataSource that EnumerateItemsMatchingFilter is being called on
	 * @param AssetDataFilter The filter to use when deciding whether a path is a valid folder
	 * @param InCallback The callback function supplied by EnumerateItemsMatchingFilter()
	 * @param SubPathEnumeration A function that calls its input function on all subpaths of the given input path, optionally recursive
	 * @param CreateFolderItem A function that generates an FContentBrowserItemData folder for the given input path
	 *
	 * @return 
	 */
	static void EnumerateFoldersMatchingFilter(UContentBrowserDataSource* DataSource, const FContentBrowserCompiledAssetDataFilter* AssetDataFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback, FSubPathEnumerationFunc SubPathEnumeration, FCreateFolderItemFunc CreateFolderItem);
	/**
	 * Call in DoesItemPassFilter() to check if a folder passes the compiled asset data filter.
	 * It is the caller's responsibility to verify EContentBrowserItemTypeFilter::IncludeFolders is set before enumerating.
	 * 
	 * @param DataSource The DataSource that DoesItemPassFilter() is being called on
	 * @param InItem The folder item to test against the filter supplied by DoesItemPassFilter()
	 * @param Filter The compiled filter to test with supplied by DoesItemPassFilter()
	 *
	 * @return true if the folder passes the filter
	 */
	static bool DoesItemPassFolderFilter(UContentBrowserDataSource* DataSource, const FContentBrowserItemData& InItem, const FContentBrowserCompiledAssetDataFilter& Filter);

	virtual void CompileFilter(const FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter) override;

	virtual void EnumerateItemsMatchingFilter(const FContentBrowserDataCompiledFilter& InFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;

	virtual void EnumerateItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;

	virtual bool EnumerateItemsAtPaths(const TArrayView<FContentBrowserItemPath> InPaths, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;

	virtual bool IsDiscoveringItems(FText* OutStatus = nullptr) override;

	virtual bool PrioritizeSearchPath(const FName InPath) override;

	virtual bool IsFolderVisibleIfHidingEmpty(const FName InPath) override;

	virtual bool CanCreateFolder(const FName InPath, FText* OutErrorMsg) override;

	virtual bool CreateFolder(const FName InPath, FContentBrowserItemDataTemporaryContext& OutPendingItem) override;

	virtual bool DoesItemPassFilter(const FContentBrowserItemData& InItem, const FContentBrowserDataCompiledFilter& InFilter) override;

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

	virtual bool CanDuplicateItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;

	virtual bool DuplicateItem(const FContentBrowserItemData& InItem, FContentBrowserItemDataTemporaryContext& OutPendingItem) override;

	virtual bool BulkDuplicateItems(TArrayView<const FContentBrowserItemData> InItems, TArray<FContentBrowserItemData>& OutNewItems) override;

	virtual bool CanSaveItem(const FContentBrowserItemData& InItem, const EContentBrowserItemSaveFlags InSaveFlags, FText* OutErrorMsg) override;

	virtual bool SaveItem(const FContentBrowserItemData& InItem, const EContentBrowserItemSaveFlags InSaveFlags) override;

	virtual bool BulkSaveItems(TArrayView<const FContentBrowserItemData> InItems, const EContentBrowserItemSaveFlags InSaveFlags) override;

	virtual bool CanDeleteItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;

	virtual bool DeleteItem(const FContentBrowserItemData& InItem) override;

	virtual bool BulkDeleteItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanPrivatizeItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg);

	virtual bool PrivatizeItem(const FContentBrowserItemData& InItem) override;

	virtual bool BulkPrivatizeItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanRenameItem(const FContentBrowserItemData& InItem, const FString* InNewName, FText* OutErrorMsg) override;

	virtual bool RenameItem(const FContentBrowserItemData& InItem, const FString& InNewName, FContentBrowserItemData& OutNewItem) override;

	virtual bool CanCopyItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg) override;

	virtual bool CopyItem(const FContentBrowserItemData& InItem, const FName InDestPath) override;

	virtual bool BulkCopyItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath) override;

	virtual bool CanMoveItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg) override;

	virtual bool MoveItem(const FContentBrowserItemData& InItem, const FName InDestPath) override;

	virtual bool BulkMoveItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath) override;

	virtual bool AppendItemReference(const FContentBrowserItemData& InItem, FString& InOutStr) override;

	virtual bool UpdateThumbnail(const FContentBrowserItemData& InItem, FAssetThumbnail& InThumbnail) override;

	virtual bool HandleDragEnterItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent) override;

	virtual bool HandleDragOverItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent) override;

	virtual bool HandleDragLeaveItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent) override;

	virtual bool HandleDragDropOnItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent) override;

	virtual bool TryGetCollectionId(const FContentBrowserItemData& InItem, FSoftObjectPath& OutCollectionId) override;

	virtual bool Legacy_TryGetPackagePath(const FContentBrowserItemData& InItem, FName& OutPackagePath) override;

	virtual bool Legacy_TryGetAssetData(const FContentBrowserItemData& InItem, FAssetData& OutAssetData) override;

	virtual bool Legacy_TryConvertPackagePathToVirtualPath(const FName InPackagePath, FName& OutPath) override;

	virtual bool Legacy_TryConvertAssetDataToVirtualPath(const FAssetData& InAssetData, const bool InUseFolderPaths, FName& OutPath) override;

	static bool PathPassesCompiledDataFilter(const FContentBrowserCompiledAssetDataFilter& InFilter, const FName InPath);

protected:
	virtual void BuildRootPathVirtualTree() override;

private:
	bool IsKnownContentPath(const FName InPackagePath) const;

	bool IsRootContentPath(const FName InPackagePath) const;

	static bool GetObjectPathsForCollections(ICollectionManager* CollectionManager, TArrayView<const FCollectionNameType> InCollections, const bool bIncludeChildCollections, TArray<FSoftObjectPath>& OutObjectPaths);

	FContentBrowserItemData CreateAssetFolderItem(const FName InFolderPath);

	FContentBrowserItemData CreateAssetFileItem(const FAssetData& InAssetData);

	TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> GetAssetFolderItemPayload(const FContentBrowserItemData& InItem) const;

	TSharedPtr<const FContentBrowserAssetFileItemDataPayload> GetAssetFileItemPayload(const FContentBrowserItemData& InItem) const;

	bool CanHandleDragDropEvent(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent) const;

	void OnAssetRegistryFileLoadProgress(const IAssetRegistry::FFileLoadProgressUpdateData& InProgressUpdateData);

	void OnAssetAdded(const FAssetData& InAssetData);

	void OnAssetRemoved(const FAssetData& InAssetData);

	void OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath);

	void OnAssetUpdated(const FAssetData& InAssetData);

	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);

	void OnPathAdded(const FString& InPath);

	void OnPathRemoved(const FString& InPath);

	void OnPathPopulated(const FName InPath);

	void OnPathPopulated(const FStringView InPath);

	void OnAlwaysShowPath(const FString& InPath);

	void OnScanCompleted();

	void OnContentPathMounted(const FString& InAssetPath, const FString& InFileSystemPath);

	void OnContentPathDismounted(const FString& InAssetPath, const FString& InFileSystemPath);

	void PopulateAddNewContextMenu(UToolMenu* InMenu);

	void PopulateContentBrowserToolBar(UToolMenu* InMenu);

	void PopulateAssetFolderContextMenu(UToolMenu* InMenu);

	void PopulateAssetFileContextMenu(UToolMenu* InMenu);

	void PopulateDragDropContextMenu(UToolMenu* InMenu);

	void OnAdvancedCopyRequested(const TArray<FName>& InAdvancedCopyInputs, const FString& InDestinationPath);

	void OnImportAsset(const FName InPath);

	void OnNewAssetRequested(const FName InPath, TWeakObjectPtr<UClass> InFactoryClass, UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation InOnBeginItemCreation);

	void OnBeginCreateAsset(const FName InDefaultAssetName, const FName InPackagePath, UClass* InAssetClass, UFactory* InFactory, UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation InOnBeginItemCreation);

	bool OnValidateItemName(const FContentBrowserItemData& InItem, const FString& InProposedName, FText* OutErrorMsg);

	FReply OnImportClicked(const UContentBrowserToolbarMenuContext* ContextObject);

	bool IsImportEnabled(const UContentBrowserToolbarMenuContext* ContextObject) const;

	FContentBrowserItemData OnFinalizeCreateFolder(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg);

	FContentBrowserItemData OnFinalizeCreateAsset(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg);

	FContentBrowserItemData OnFinalizeDuplicateAsset(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg);

	IAssetRegistry* AssetRegistry;

	IAssetTools* AssetTools;

	ICollectionManager* CollectionManager;

	TSharedPtr<FAssetFolderContextMenu> AssetFolderContextMenu;

	TSharedPtr<FAssetFileContextMenu> AssetFileContextMenu;

	FText DiscoveryStatusText;

	/**
	 * The array of known root content paths that can hold assets.
	 * @note These paths include a trailing slash.
	 */
	TArray<FString> RootContentPaths;

	/**
	 * The set of folders that should always be visible, even if they contain no assets in the Content Browser view.
	 * This will include root content folders, and any folders that have been created directly (or indirectly) by a user action.
	 */
	TSet<FString> AlwaysVisibleAssetFolders;

	/**
	 * A cache of folders that contain no assets in the Content Browser view.
	 */
	TSet<FString> EmptyAssetFolders;
};
