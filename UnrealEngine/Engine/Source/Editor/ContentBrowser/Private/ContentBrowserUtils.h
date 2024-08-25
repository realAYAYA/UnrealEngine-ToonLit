// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetViewUtils.h"
#include "CollectionManagerTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "Framework/SlateDelegates.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FPathPermissionList;
class FSlateRect;
class SAssetView;
class SPathView;
class SWidget;
struct FARFilter;
struct FAssetData;
struct FContentBrowserDataFilter;
struct FContentBrowserItem;
struct FContentBrowserItemPath;
enum class EContentBrowserIsFolderVisibleFlags : uint8;

namespace ContentBrowserUtils
{
	// Import the functions that were moved into the more common AssetViewUtils namespace
	using namespace AssetViewUtils;

	/** Displays a modeless message at the specified anchor. It is fine to specify a zero-size anchor, just use the top and left fields */
	void DisplayMessage(const FText& Message, const FSlateRect& ScreenAnchor, const TSharedRef<SWidget>& ParentContent);

	/** Displays a modeless message asking yes or no type question */
	void DisplayConfirmationPopup(const FText& Message, const FText& YesString, const FText& NoString, const TSharedRef<SWidget>& ParentContent, const FOnClicked& OnYesClicked, const FOnClicked& OnNoClicked = FOnClicked());

	/** Returns references to the specified items */
	FString GetItemReferencesText(const TArray<FContentBrowserItem>& Items);

	/** Returns references to the specified folders */
	FString GetFolderReferencesText(const TArray<FContentBrowserItem>& Folders);

	/** Copies references to the specified items to the clipboard */
	void CopyItemReferencesToClipboard(const TArray<FContentBrowserItem>& ItemsToCopy);

	/** Copies references to the specified folders to the clipboard */
	void CopyFolderReferencesToClipboard(const TArray<FContentBrowserItem>& FoldersToCopy);

	/** Copies file paths on disk to the specified items to the clipboard */
	void CopyFilePathsToClipboard(const TArray<FContentBrowserItem>& ItemsToCopy);

	/** Check whether the given item is considered to be developer content */
	bool IsItemDeveloperContent(const FContentBrowserItem& InItem);

	/** Check whether the given item is considered to be localized content */
	bool IsItemLocalizedContent(const FContentBrowserItem& InItem);

	/** Check whether the given item is considered to be engine content (including engine plugins) */
	bool IsItemEngineContent(const FContentBrowserItem& InItem);

	/** Check whether the given item is considered to be project content (including project plugins) */
	bool IsItemProjectContent(const FContentBrowserItem& InItem);

	/** Check whether the given item is considered to be plugin content (engine or project) */
	bool IsItemPluginContent(const FContentBrowserItem& InItem);

	/** Check whether the given item is the root folder of a plugin */
	bool IsItemPluginRootFolder(const FContentBrowserItem& InItem);

	/** Check to see whether the given path is rooted against a collection directory, optionally extracting the collection name and share type from the path */
	bool IsCollectionPath(const FString& InPath, FName* OutCollectionName = nullptr, ECollectionShareType::Type* OutCollectionShareType = nullptr);

	/** Given an array of paths, work out how many are rooted against class roots, and how many are rooted against asset roots */
	void CountPathTypes(const TArray<FString>& InPaths, int32& OutNumAssetPaths, int32& OutNumClassPaths);

	/** Given an array of paths, work out how many are rooted against class roots, and how many are rooted against asset roots */
	void CountPathTypes(const TArray<FName>& InPaths, int32& OutNumAssetPaths, int32& OutNumClassPaths);

	/** Given an array of "asset" data, work out how many are assets, and how many are classes */
	void CountItemTypes(const TArray<FAssetData>& InItems, int32& OutNumAssetItems, int32& OutNumClassItems);

	/** Gets the platform specific text for the "explore" command (FPlatformProcess::ExploreFolder) */
	FText GetExploreFolderText();

	/** Perform a batched "explore" operation on the specified file and/or folder paths */
	void ExploreFolders(const TArray<FContentBrowserItem>& InItems, const TSharedRef<SWidget>& InParentContent);

	/** Returns if can perform a batched "explore" operation on the specified file and/or folder paths */
	bool CanExploreFolders(const TArray<FContentBrowserItem>& InItems);

	/** Convert a legacy asset and path selection to their corresponding virtual paths for content browser data items */
	void ConvertLegacySelectionToVirtualPaths(TArrayView<const FAssetData> InAssets, TArrayView<const FString> InFolders, const bool InUseFolderPaths, TArray<FName>& OutVirtualPaths);
	void ConvertLegacySelectionToVirtualPaths(TArrayView<const FAssetData> InAssets, TArrayView<const FString> InFolders, const bool InUseFolderPaths, TSet<FName>& OutVirtualPaths);

	/** Append the asset registry filter and permission lists to the content browser data filter */
	void AppendAssetFilterToContentBrowserFilter(const FARFilter& InAssetFilter, const TSharedPtr<FPathPermissionList>& InAssetClassPermissionList, const TSharedPtr<FPathPermissionList>& InFolderPermissionList, FContentBrowserDataFilter& OutDataFilter);

	/* Combine folder filters into a new filter if either are active */
	TSharedPtr<FPathPermissionList> GetCombinedFolderPermissionList(const TSharedPtr<FPathPermissionList>& FolderPermissionList, const TSharedPtr<FPathPermissionList>& WritableFolderPermissionList);

	/** Shared logic to know if we can perform certain operation depending on which view it occurred, either PathView or AssetView */
	bool CanDeleteFromAssetView(TWeakPtr<SAssetView> AssetView, FText* OutErrorMsg = nullptr);
	bool CanRenameFromAssetView(TWeakPtr<SAssetView> AssetView, FText* OutErrorMsg = nullptr);
	bool CanDeleteFromPathView(TWeakPtr<SPathView> PathView, FText* OutErrorMsg = nullptr);
	bool CanRenameFromPathView(TWeakPtr<SPathView> PathView, FText* OutErrorMsg = nullptr);

	/** Returns internal path if it has one, otherwise strips /All prefix from virtual path*/
	FName GetInvariantPath(const FContentBrowserItemPath& ItemPath);

	/** Get the set of flags to use with IsFolderVisible */
	EContentBrowserIsFolderVisibleFlags GetIsFolderVisibleFlags(const bool bDisplayEmpty);

	/** Returns if this folder has been marked as a favorite folder */
	UE_DEPRECATED(5.3, "Use function that takes FContentBrowserItemPath instead.")
	bool IsFavoriteFolder(const FString& FolderPath);
	bool IsFavoriteFolder(const FContentBrowserItemPath& FolderPath);

	UE_DEPRECATED(5.3, "Use function that takes FContentBrowserItemPath instead.")
	void AddFavoriteFolder(const FString& FolderPath, bool bFlushConfig = true);
	void AddFavoriteFolder(const FContentBrowserItemPath& FolderPath);

	UE_DEPRECATED(5.3, "Use function that takes FContentBrowserItemPath instead.")
	void RemoveFavoriteFolder(const FString& FolderPath, bool bFlushConfig = true);
	void RemoveFavoriteFolder(const FContentBrowserItemPath& FolderPath);

	const TArray<FString>& GetFavoriteFolders();

	/** Adds FolderPath as a private content edit folder if it's allowed to be toggled as such */
	void AddShowPrivateContentFolder(const FStringView VirtualFolderPath, const FName Owner);

	/** Removes FolderPath as a private content edit folder if it's allowed to be toggled as such */
	void RemoveShowPrivateContentFolder(const FStringView VirtualFolderPath, const FName Owner);

	/** Returns whether we should display icons for custom virtual folders in the content browser */
	bool ShouldShowCustomVirtualFolderIcon();

	/** Returns whether we should display icons for plugins in the content browser */
	bool ShouldShowPluginFolderIcon();
}
