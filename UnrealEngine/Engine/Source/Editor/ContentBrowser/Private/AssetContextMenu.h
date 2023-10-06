// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "ContentBrowserDataMenuContexts.h"
#include "ContentBrowserItem.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "SourcesData.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FUICommandList;
class SAssetView;
class SWidget;
class UClass;
class UToolMenu;

enum class ECheckBoxState : uint8;
enum class EContentBrowserViewContext : uint8;

class FAssetContextMenu : public TSharedFromThis<FAssetContextMenu>
{
public:
	/** Constructor */
	FAssetContextMenu(const TWeakPtr<SAssetView>& InAssetView);

	/** Binds the commands used by the asset view context menu to the content browser command list */
	void BindCommands(TSharedPtr< FUICommandList >& Commands);

	/** Makes the context menu widget */
	TSharedRef<SWidget> MakeContextMenu(TArrayView<const FContentBrowserItem> InSelectedItems, const FSourcesData& InSourcesData, TSharedPtr< FUICommandList > InCommandList);

	/** Updates the list of currently selected items to those passed in */
	void SetSelectedItems(TArrayView<const FContentBrowserItem> InSelectedItems);

	/** Read-only access to the list of currently selected items */
	const TArray<FContentBrowserItem>& GetSelectedItems() const { return SelectedItems; }

	using FOnShowInPathsViewRequested = UContentBrowserDataMenuContext_FileMenu::FOnShowInPathsView;
	void SetOnShowInPathsViewRequested(const FOnShowInPathsViewRequested& InOnShowInPathsViewRequested);

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE_TwoParams(FOnRenameRequested, const FContentBrowserItem& /*ItemToRename*/, EContentBrowserViewContext /*ViewContext*/);
	void SetOnRenameRequested(const FOnRenameRequested& InOnRenameRequested);

	/** Delegate for when the context menu requests an item duplication */
	DECLARE_DELEGATE_OneParam(FOnDuplicateRequested, TArrayView<const FContentBrowserItem> /*OriginalItems*/);
	void SetOnDuplicateRequested(const FOnDuplicateRequested& InOnDuplicateRequested);

	/** Delegate for when the context menu requests to edit an item */
	DECLARE_DELEGATE_OneParam(FOnEditRequested, TArrayView<const FContentBrowserItem> /*Items*/);
	void SetOnEditRequested(const FOnEditRequested& InOnEditRequested);

	/** Delegate for when the context menu requests an asset view refresh */
	using FOnAssetViewRefreshRequested = UContentBrowserDataMenuContext_FileMenu::FOnRefreshView;
	void SetOnAssetViewRefreshRequested(const FOnAssetViewRefreshRequested& InOnAssetViewRefreshRequested);

	/** Handler to check to see if a rename command is allowed */
	bool CanExecuteRename() const;

	/** Handler for Rename */
	void ExecuteRename(EContentBrowserViewContext ViewContext);

	/** Handler to check to see if a delete command is allowed */
	bool CanExecuteDelete() const;

	/** Handler for Delete */
	void ExecuteDelete();

	/** Handler to check to see if "Save Asset" can be executed */
	bool CanExecuteSaveAsset() const;

	/** Handler for when "Save Asset" is selected */
	void ExecuteSaveAsset();

private:
	/** Handler to check to see if a duplicate command is allowed */
	bool CanExecuteDuplicate() const;

	/** Handler for Duplicate */
	void ExecuteDuplicate();

	/** Registers all unregistered menus in the hierarchy for a class */
	static void RegisterMenuHierarchy(UClass* InClass);

	/** Adds options to menu */
	void AddMenuOptions(UToolMenu* InMenu);

	/** Adds asset type-specific menu options to a menu builder. Returns true if any options were added. */
	bool AddAssetTypeMenuOptions(UToolMenu* Menu);
	
	/** Adds common menu options to a menu builder. Returns true if any options were added. */
	bool AddCommonMenuOptions(UToolMenu* Menu);

	/** Adds explore menu options to a menu builder. */
	void AddExploreMenuOptions(UToolMenu* Menu);

	/** Adds asset reference menu options to a menu builder. Returns true if any options were added. */
	bool AddReferenceMenuOptions(UToolMenu* Menu);

	bool AddPublicStateMenuOptions(UToolMenu* Menu);

	/** Adds menu options related to working with collections */
	bool AddCollectionMenuOptions(UToolMenu* Menu);

	/** Handler for when sync to asset tree is selected */
	void ExecuteSyncToAssetTree();

	/** Handler for when find in explorer is selected */
	void ExecuteFindInExplorer();

	/** Handler to check to see if an edit command is allowed */
	bool CanExecuteEditItems() const;

	/** Handler for when "Edit" is selected */
	void ExecuteEditItems();

	/** Handler for confirmation of folder deletion */
	FReply ExecuteDeleteFolderConfirmed();

	/** Get tooltip for delete */
	FText GetDeleteToolTip() const;

	/** Handler for when "Public Asset" is toggled */
	void ExecutePublicAssetToggle();

	/** Handler to check to see if a Public Asset toggle is allowed */
	bool CanExecutePublicAssetToggle();

	/** Handler for setting all selected assets to Public */
	void ExecuteBulkSetPublicAsset();

	/** Handler for setting all selected assets to Private */
	void ExecuteBulkUnsetPublicAsset();

	/** Handler to check if all selected assets can have their Public state changed */
	bool CanExecuteBulkSetPublicAsset();

	/** Handler for determining the selected asset's Public state */
	bool IsSelectedAssetPublic();

	/** Handler for determining the selected asset's Private state */
	bool IsSelectedAssetPrivate();

	/** Handler for CopyReference */
	void ExecuteCopyReference();

	/** Handler for CopyFilePath */
	void ExecuteCopyFilePath();

	/** Handler for when "Remove from collection" is selected */
	void ExecuteRemoveFromCollection();

	/** Handler to check to see if a sync to asset tree command is allowed */
	bool CanExecuteSyncToAssetTree() const;

	/** Handler to check to see if a find in explorer command is allowed */
	bool CanExecuteFindInExplorer() const;	

	/** Handler to check to see if a "Remove from collection" command is allowed */
	bool CanExecuteRemoveFromCollection() const;

	/** Initializes some variable used to in "CanExecute" checks that won't change at runtime or are too expensive to check every frame. */
	void CacheCanExecuteVars();

	/** Registers the base context menu for assets */
	void RegisterContextMenu(const FName MenuName);

	TArray<FContentBrowserItem> SelectedItems;
	TArray<FContentBrowserItem> SelectedFiles;
	TArray<FContentBrowserItem> SelectedFolders;

	FSourcesData SourcesData;

	/** The asset view this context menu is a part of */
	TWeakPtr<SAssetView> AssetView;

	FOnShowInPathsViewRequested OnShowInPathsViewRequested;
	FOnRenameRequested OnRenameRequested;
	FOnDuplicateRequested OnDuplicateRequested;
	FOnEditRequested OnEditRequested;
	FOnAssetViewRefreshRequested OnAssetViewRefreshRequested;

	/** Cached CanExecute vars */
	bool bCanExecuteFindInExplorer = false;
	bool bCanExecutePublicAssetToggle = false;
	bool bCanExecuteBulkSetPublicAsset = false;
};
