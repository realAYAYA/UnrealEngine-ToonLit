// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataMenuContexts.h"
#include "ContentBrowserFileDataPayload.h"
#include "ContentBrowserFileDataSource.generated.h"

class UToolMenu;
class FContentBrowserFileDataDiscovery;
class IAssetTypeActions;
struct FFileChangeData;

USTRUCT()
struct CONTENTBROWSERFILEDATASOURCE_API FContentBrowserCompiledFileDataFilter
{
	GENERATED_BODY()

public:
	FName VirtualPathName;
	FString VirtualPath;
	bool bRecursivePaths = false;
	EContentBrowserItemAttributeFilter ItemAttributeFilter = EContentBrowserItemAttributeFilter::IncludeNone;
	TSharedPtr<FPathPermissionList> PermissionList;
	TArray<FString> FileExtensionsToInclude;
};

UCLASS()
class CONTENTBROWSERFILEDATASOURCE_API UContentBrowserFileDataSource : public UContentBrowserDataSource
{
	GENERATED_BODY()

public:
	void Initialize(const ContentBrowserFileData::FFileConfigData& InConfig, const bool InAutoRegister = true);

	virtual void Shutdown() override;

	void AddFileMount(const FName InFileMountPath, const FString& InFileMountDiskPath);

	void RemoveFileMount(const FName InFileMountPath);

	virtual void Tick(const float InDeltaTime);

	virtual void CompileFilter(const FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter) override;

	virtual void EnumerateItemsMatchingFilter(const FContentBrowserDataCompiledFilter& InFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;

	virtual void EnumerateItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) override;

	virtual bool CanCreateFolder(const FName InPath, FText* OutErrorMsg) override;

	virtual bool CreateFolder(const FName InPath, FContentBrowserItemDataTemporaryContext& OutPendingItem) override;

	virtual bool IsDiscoveringItems(FText* OutStatus = nullptr) override;

	virtual bool PrioritizeSearchPath(const FName InPath) override;

	virtual bool IsFolderVisibleIfHidingEmpty(const FName InPath) override;

	virtual bool DoesItemPassFilter(const FContentBrowserItemData& InItem, const FContentBrowserDataCompiledFilter& InFilter) override;

	virtual bool GetItemAttribute(const FContentBrowserItemData& InItem, const bool InIncludeMetaData, const FName InAttributeKey, FContentBrowserItemDataAttributeValue& OutAttributeValue) override;

	virtual bool GetItemAttributes(const FContentBrowserItemData& InItem, const bool InIncludeMetaData, FContentBrowserItemDataAttributeValues& OutAttributeValues) override;

	virtual bool GetItemPhysicalPath(const FContentBrowserItemData& InItem, FString& OutDiskPath) override;

	virtual bool CanEditItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;

	virtual bool EditItem(const FContentBrowserItemData& InItem) override;

	virtual bool BulkEditItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanPreviewItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;

	virtual bool PreviewItem(const FContentBrowserItemData& InItem) override;

	virtual bool BulkPreviewItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanDuplicateItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;

	virtual bool DuplicateItem(const FContentBrowserItemData& InItem, FContentBrowserItemDataTemporaryContext& OutPendingItem) override;

	virtual bool BulkDuplicateItems(TArrayView<const FContentBrowserItemData> InItems, TArray<FContentBrowserItemData>& OutNewItems) override;

	virtual bool CanDeleteItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg) override;

	virtual bool DeleteItem(const FContentBrowserItemData& InItem) override;

	virtual bool BulkDeleteItems(TArrayView<const FContentBrowserItemData> InItems) override;

	virtual bool CanRenameItem(const FContentBrowserItemData& InItem, const FString* InNewName, FText* OutErrorMsg) override;

	virtual bool RenameItem(const FContentBrowserItemData& InItem, const FString& InNewName, FContentBrowserItemData& OutNewItem) override;

	virtual bool CanCopyItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg) override;

	virtual bool CopyItem(const FContentBrowserItemData& InItem, const FName InDestPath) override;

	virtual bool BulkCopyItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath) override;

	virtual bool CanMoveItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg) override;

	virtual bool MoveItem(const FContentBrowserItemData& InItem, const FName InDestPath) override;

	virtual bool BulkMoveItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath) override;

	virtual bool UpdateThumbnail(const FContentBrowserItemData& InItem, FAssetThumbnail& InThumbnail) override;

	virtual void BuildRootPathVirtualTree() override;

protected:

	struct FFileMount
	{
		FString DiskPath;
		FDelegateHandle DirectoryWatcherHandle;
	};

	struct FDiscoveredItem
	{
		enum class EType : uint8
		{
			Directory,
			File,
		};

		EType Type;
		FString DiskPath;
		TSet<FName> ChildItems;
	};

	FContentBrowserItemData CreateFolderItem(const FName InInternalPath, const FString& InFilename);

	FContentBrowserItemData CreateFileItem(const FName InInternalPath, const FString& InFilename);

	FContentBrowserItemData CreateItemFromDiscovered(const FName InInternalPath, const FDiscoveredItem& InDiscoveredItem);

	TSharedPtr<const FContentBrowserFolderItemDataPayload> GetFolderItemPayload(const FContentBrowserItemData& InItem) const;

	TSharedPtr<const FContentBrowserFileItemDataPayload> GetFileItemPayload(const FContentBrowserItemData& InItem) const;

	bool IsKnownFileMount(const FName InMountPath, FString* OutDiskPath = nullptr) const;

	bool IsRootFileMount(const FName InMountPath, FString* OutDiskPath = nullptr) const;

	void AddDiscoveredItem(FDiscoveredItem::EType InItemType, const FString& InMountPath, const FString& InDiskPath, const bool bIsRootPath);
	void AddDiscoveredItemImpl(FDiscoveredItem::EType InItemType, const FString& InMountPath, const FString& InDiskPath, const FName InChildMountPathName, const bool bIsRootPath);

	void RemoveDiscoveredItem(const FString& InMountPath);
	void RemoveDiscoveredItem(const FName InMountPath);
	void RemoveDiscoveredItemImpl(const FName InMountPath, const bool bParentIsOrphan);

	void OnPathPopulated(const FName InPath);
	void OnPathPopulated(const FStringView InPath);

	void OnAlwaysShowPath(const FName InPath);
	void OnAlwaysShowPath(const FStringView InPath);

	/** Called when a file in a watched directory changes on disk */
	void OnDirectoryChanged(const TArray<FFileChangeData>& InFileChanges, const FString InFileMountPath, const FString InFileMountDiskPath);

	void PopulateAddNewContextMenu(UToolMenu* InMenu);

	void OnNewFileRequested(const FName InDestFolderPath, const FString InDestFolder, TSharedRef<const ContentBrowserFileData::FFileActions> InFileActions, UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation InOnBeginItemCreation);

	bool OnValidateItemName(const FContentBrowserItemData& InItem, const FString& InProposedName, FText* OutErrorMsg);

	FContentBrowserItemData OnFinalizeCreateFolder(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg);

	FContentBrowserItemData OnFinalizeCreateFile(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg);

	FContentBrowserItemData OnFinalizeDuplicateFile(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg);

	bool PassesFilters(const FName InPath, const int32 InFolderDepthChecked, const FContentBrowserCompiledFileDataFilter& InFileDataFilter) const;
	static bool PassesFilters(const FStringView InPath, const FDiscoveredItem& InDiscoveredItem, const int32 InFolderDepthChecked, const FContentBrowserCompiledFileDataFilter& InFileDataFilter);

	ContentBrowserFileData::FFileConfigData Config;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	TSortedMap<FName, FFileMount, FDefaultAllocator, FNameFastLess> RegisteredFileMounts;
	TMap<FName, TArray<FName>> RegisteredFileMountRoots;

	TMap<FName, FDiscoveredItem> DiscoveredItems;

	TSharedPtr<FContentBrowserFileDataDiscovery> BackgroundDiscovery;

	/**
	 * The set of folders that should always be visible, even if they contain no files in the Content Browser view.
	 * This will include root file mounts, and any folders that have been created directly (or indirectly) by a user action.
	 */
	TSet<FString> AlwaysVisibleFolders;

	/**
	 * A cache of folders that contain no files in the Content Browser view.
	 */
	TSet<FString> EmptyFolders;
};
