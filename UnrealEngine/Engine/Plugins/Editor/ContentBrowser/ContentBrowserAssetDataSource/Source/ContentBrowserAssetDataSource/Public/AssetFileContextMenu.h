// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserDataMenuContexts.h"
#include "Input/Reply.h"

class UToolMenu;

class CONTENTBROWSERASSETDATASOURCE_API FAssetFileContextMenu : public TSharedFromThis<FAssetFileContextMenu>
{
public:
	DECLARE_DELEGATE_OneParam(FOnShowAssetsInPathsView, const TArray<FAssetData>& /*AssetsToFind*/);

	/** Makes the context menu widget */
	void MakeContextMenu(
		UToolMenu* InMenu,
		const TArray<FAssetData>& InSelectedAssets,
		const FOnShowAssetsInPathsView& InOnShowAssetsInPathsView
		);

private:
	struct FSourceAssetsState
	{
		TSet<FSoftObjectPath> SelectedAssets;
		TSet<FSoftObjectPath> CurrentAssets;
	};

	struct FLocalizedAssetsState
	{
		FCulturePtr Culture;
		TSet<FSoftObjectPath> NewAssets;
		TSet<FSoftObjectPath> CurrentAssets;
	};

private:
	/** Helper to load selected assets and sort them by UClass */
	void GetSelectedAssetsByClass(TMap<UClass*, TArray<UObject*> >& OutSelectedAssetsByClass) const;

	/** Helper to collect resolved filepaths for all selected assets */
	void GetSelectedAssetSourceFilePaths(TArray<FString>& OutFilePaths, TArray<FString>& OutUniqueSourceFileLabels, int32 &OutValidSelectedAssetCount) const;

	/** Handler to check to see if a imported asset actions should be visible in the menu */
	bool AreImportedAssetActionsVisible() const;

	/** Handler to check to see if imported asset actions are allowed */
	bool CanExecuteImportedAssetActions(const TArray<FString> ResolvedFilePaths) const;

	/** Handler to check to see if reimport asset actions are allowed */
	bool CanExecuteReimportAssetActions(const TArray<FString> ResolvedFilePaths) const;

	/** Handler for Reimport */
	void ExecuteReimport(int32 SourceFileIndex = INDEX_NONE);

	void ExecuteReimportWithNewFile(int32 SourceFileIndex = INDEX_NONE);

	/** Handler for FindInExplorer */
	void ExecuteFindSourceInExplorer(const TArray<FString> ResolvedFilePaths);

	/** Handler for OpenInExternalEditor */
	void ExecuteOpenInExternalEditor(const TArray<FString> ResolvedFilePaths);

	/** Handler to check to see if a reload command is allowed */
	bool CanExecuteReload() const;

	/** Handler for Reload */
	void ExecuteReload();

	/** Is allowed to modify files or folders under this path */
	bool CanModifyPath(const FString& InPath) const;

	/** Adds options to menu */
	void AddMenuOptions(UToolMenu* Menu);

	/** Adds asset type-specific menu options to a menu builder. Returns true if any options were added. */
	bool AddImportedAssetMenuOptions(UToolMenu* Menu);
	
	/** Adds common menu options to a menu builder. Returns true if any options were added. */
	bool AddCommonMenuOptions(UToolMenu* Menu);

	/** Adds Asset Actions sub-menu to a menu builder. */
	void MakeAssetActionsSubMenu(UToolMenu* Menu);

	/** Handler to check to see if "Asset Actions" are allowed */
	bool CanExecuteAssetActions() const;

	/** Adds Asset Localization sub-menu to a menu builder. */
	void MakeAssetLocalizationSubMenu(UToolMenu* Menu);

	/** Adds the Create Localized Asset sub-menu to a menu builder. */
	void MakeCreateLocalizedAssetSubMenu(UToolMenu* Menu, TSet<FSoftObjectPath> InSelectedSourceAssets, TArray<FLocalizedAssetsState> InLocalizedAssetsState);

	/** Adds the Show Localized Assets sub-menu to a menu builder. */
	void MakeShowLocalizedAssetSubMenu(UToolMenu* Menu, TArray<FLocalizedAssetsState> InLocalizedAssetsState);

	/** Adds the Edit Localized Assets sub-menu to a menu builder. */
	void MakeEditLocalizedAssetSubMenu(UToolMenu* Menu, TArray<FLocalizedAssetsState> InLocalizedAssetsState);

	/** Create new localized assets for the given culture */
	void ExecuteCreateLocalizedAsset(TSet<FSoftObjectPath> InSelectedSourceAssets, FLocalizedAssetsState InLocalizedAssetsStateForCulture);

	/** Find the given assets in the Content Browser */
	void ExecuteFindInAssetTree(TArray<FSoftObjectPath> InAssets);

	/** Open the given assets in their respective editors */
	void ExecuteOpenEditorsForAssets(TArray<FSoftObjectPath> InAssets);

	/** Adds asset documentation menu options to a menu builder. Returns true if any options were added. */
	bool AddDocumentationMenuOptions(UToolMenu* Menu);
	
	/** Adds source control menu options to a menu builder. */
	bool AddSourceControlMenuOptions(UToolMenu* Menu);

	/** Fills the source control sub-menu */
	void FillSourceControlSubMenu(UToolMenu* Menu);

	/** Handler to check to see if SCC actions are allowed */
	bool CanExecuteSourceControlActions() const;

	/** Creates a sub-menu of Chunk IDs that are are assigned to all selected assets */
	void MakeChunkIDListMenu(UToolMenu* Menu);

	/** Handler for when create using asset is selected */
	void ExecuteCreateBlueprintUsing();

	/** Handler for when "Find in World" is selected */
	void ExecuteFindAssetInWorld();

	/** Handler for when "Property Matrix..." is selected */
	void ExecutePropertyMatrix();

	/** Handler for when "Show MetaData" is selected */
	void ExecuteShowAssetMetaData();

	/** Handler for when "Diff Selected" is selected */
	void ExecuteDiffSelected() const;

	/** Handler for Consolidate */
	void ExecuteConsolidate();

	/** Handler for Capture Thumbnail */
	void ExecuteCaptureThumbnail();

	/** Handler for Clear Thumbnail */
	void ExecuteClearThumbnail();

	/** Handler for when "Migrate Asset" is selected */
	void ExecuteMigrateAsset();

	/** Handler for GoToAssetCode */
	void ExecuteGoToCodeForAsset(UClass* SelectedClass);

	/** Handler for GoToAssetDocs */
	void ExecuteGoToDocsForAsset(UClass* SelectedClass);

	/** Handler for GoToAssetDocs */
	void ExecuteGoToDocsForAsset(UClass* SelectedClass, const FString ExcerptSection);

	/** Handler to copy the given text to the clipboard */
	void ExecuteCopyTextToClipboard(FString InText);

	/** Handler for resetting the localization ID of the current selection */
	void ExecuteResetLocalizationId();

	/** Handler for showing the cached list of localized texts stored in the package header */
	void ExecuteShowLocalizationCache(const FString InPackageFilename);

	/** Handler for Export */
	void ExecuteExport();

	/** Handler for Bulk Export */
	void ExecuteBulkExport();

	/** Handler for when "Refresh source control" is selected */
	void ExecuteSCCRefresh();

	/** Handler for when "Merge" is selected */
	void ExecuteSCCMerge();

	/** Handler for when "Checkout from source control" is selected */
	void ExecuteSCCCheckOut();

	/** Handler for when "Open for add to source control" is selected */
	void ExecuteSCCOpenForAdd();

	/** Handler for when "Checkin to source control" is selected */
	void ExecuteSCCCheckIn();

	/** Handler for when "Source Control History" is selected */
	void ExecuteSCCHistory();

	/** Handler for when "Diff Against Depot" is selected */
	void ExecuteSCCDiffAgainstDepot() const;

	/** Handler for when "Source Control Revert" is selected */
	void ExecuteSCCRevert();

	/** Handler for when "Source Control Sync" is selected */
	void ExecuteSCCSync();

	/** Handler for when source control is disabled */
	void ExecuteEnableSourceControl();

	/** Handler to assign ChunkID to a selection of assets */
	void ExecuteAssignChunkID();

	/** Handler to remove all ChunkID assignments from a selection of assets */
	void ExecuteRemoveAllChunkID();

	/** Handler to remove a single ChunkID assignment from a selection of assets */
	void ExecuteRemoveChunkID(int32 ChunkID);

	/** Handler to export the selected asset(s) to experimental text format */
	void ExportSelectedAssetsToText();

	/** Handler to export the selected asset(s) to experimental text format */
	void ViewSelectedAssetAsText();
	bool CanViewSelectedAssetAsText() const;

	/** Run the rountrip test on this asset */
	void DoTextFormatRoundtrip();

	/** Handler to check if we can create blueprint using selected asset */
	bool CanExecuteCreateBlueprintUsing() const;

	/** Handler to check to see if a find in world command is allowed */
	bool CanExecuteFindAssetInWorld() const;

	/** Handler to check to see if a properties command is allowed */
	bool CanExecuteProperties() const;

	/** Handler to check to see if a property matrix command is allowed */
	bool CanExecutePropertyMatrix(FText& OutErrorMessage) const;
	bool CanExecutePropertyMatrix() const;

	/** Handler to check to see if a "Show MetaData" command is allowed */
	bool CanExecuteShowAssetMetaData() const;

	/** Handler to check to see if "Refresh source control" can be executed */
	bool CanExecuteSCCRefresh() const;

	/** Handler to check to see if "Merge" can be executed */
	bool CanExecuteSCCMerge() const;

	/** Handler to check to see if "Checkout from source control" can be executed */
	bool CanExecuteSCCCheckOut() const;

	/** Handler to check to see if "Open for add to source control" can be executed */
	bool CanExecuteSCCOpenForAdd() const;

	/** Handler to check to see if "Checkin to source control" can be executed */
	bool CanExecuteSCCCheckIn() const;

	/** Handler to check to see if "Source Control History" can be executed */
	bool CanExecuteSCCHistory() const;

	/** Handler to check to see if "Source Control Revert" can be executed */
	bool CanExecuteSCCRevert() const;

	/** Handler to check to see if "Source Control Sync" can be executed */
	bool CanExecuteSCCSync() const;

	/** Handler to check to see if "Diff Against Depot" can be executed */
	bool CanExecuteSCCDiffAgainstDepot() const;

	/** Handler to check to see if "Diff Selected" can be executed */
	bool CanExecuteDiffSelected() const;

	/** Handler to check to see if "Capture Thumbnail" can be executed */
	bool CanExecuteCaptureThumbnail() const;

	/** Handler to check to see if "Clear Thumbnail" should be visible */
	bool CanClearCustomThumbnails() const;

	/** Initializes some variable used to in "CanExecute" checks that won't change at runtime or are too expensive to check every frame. */
	void CacheCanExecuteVars();

	/** Helper function to gather the package names of all selected assets */
	void GetSelectedPackageNames(TArray<FString>& OutPackageNames) const;

	/** Helper function to gather the packages containing all selected assets */
	void GetSelectedPackages(TArray<UPackage*>& OutPackages) const;

	/** Update internal state logic */
	void OnChunkIDAssignChanged(int32 ChunkID);

	/** Gets the current value of the ChunkID entry box */
	TOptional<int32> GetChunkIDSelection() const;

	/** Handles when the Assign chunkID dialog OK button is clicked */
	FReply OnChunkIDAssignCommit(TSharedPtr<SWindow> Window);

	/** Handles when the Assign chunkID dialog Cancel button is clicked */
	FReply OnChunkIDAssignCancel(TSharedPtr<SWindow> Window);

	/** Generates tooltip for the Property Matrix menu option */
	FText GetExecutePropertyMatrixTooltip() const;

	/** Generates a list of selected assets in the content browser */
	void GetSelectedAssets(TArray<UObject*>& Assets, bool SkipRedirectors) const;

private:
	TArray<FAssetData> SelectedAssets;

	TWeakPtr<SWidget> ParentWidget;

	FOnShowAssetsInPathsView OnShowAssetsInPathsView;

	UContentBrowserDataMenuContext_FileMenu::FOnRefreshView OnRefreshView;

	/** Cached CanExecute vars */
	bool bAtLeastOneNonRedirectorSelected = false;
	bool bCanExecuteSCCMerge = false;
	bool bCanExecuteSCCCheckOut = false;
	bool bCanExecuteSCCOpenForAdd = false;
	bool bCanExecuteSCCCheckIn = false;
	bool bCanExecuteSCCHistory = false;
	bool bCanExecuteSCCRevert = false;
	bool bCanExecuteSCCSync = false;
	/** */
	int32 ChunkIDSelected = 0;
};
