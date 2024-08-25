// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/CurveSequence.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetThumbnail.h"
#include "AssetViewSortManager.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDataLegacyBridge.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserTelemetry.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/Platform.h"
#include "HistoryManager.h"
#include "IContentBrowserSingleton.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/NamePermissionList.h"
#include "Misc/Optional.h"
#include "SourcesData.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Templates/UniquePtr.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

class FAssetThumbnail;
class FAssetViewItem;
class FContentBrowserItemDataTemporaryContext;
class FContentBrowserItemDataUpdate;
class FDragDropEvent;
class FMenuBuilder;
class FPathPermissionList;
class FSlateRect;
class FWeakWidgetPath;
class FWidgetPath;
class ITableRow;
class SAssetColumnView;
class SAssetListView;
class SAssetTileView;
class SBox;
class SComboButton;
class SContentBrowser;
class SFilterList;
class STableViewBase;
class SWidget;
class UClass;
class UFactory;
class UToolMenu;
struct FAssetViewInstanceConfig;
struct FCharacterEvent;
struct FCollectionNameType;
struct FContentBrowserInstanceConfig;
struct FFocusEvent;
struct FGeometry;
struct FKeyEvent;
struct FPointerEvent;
struct FPropertyChangedEvent;
struct FToolMenuContext;

/** Fires whenever the asset view is asked to start to create a temporary item */
DECLARE_DELEGATE_OneParam(FOnAssetViewNewItemRequested, const FContentBrowserItem& /*NewItem*/);

/** Fires whenever one of the "Search" options changes, useful for modifying search criteria to match */
DECLARE_DELEGATE(FOnSearchOptionChanged);

/** Fires whenever asset view options menu is being opened, gives chance for external code to set additional context */
DECLARE_DELEGATE_OneParam(FOnExtendAssetViewOptionsMenuContext, FToolMenuContext&);

/**
 * A widget to display a list of filtered assets
 */
class CONTENTBROWSER_API SAssetView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAssetView )
		: _InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAll)
		, _ThumbnailLabel( EThumbnailLabel::ClassName )
		, _AllowThumbnailHintLabel(true)
		, _bShowPathViewFilters(false)
		, _InitialViewType(EAssetViewType::Tile)
		, _InitialThumbnailSize(EThumbnailSize::Medium)
		, _ShowBottomToolbar(true)
		, _ShowViewOptions(true)
		, _AllowThumbnailEditMode(false)
		, _CanShowClasses(true)
		, _CanShowFolders(false)
		, _CanShowReadOnlyFolders(true)
		, _FilterRecursivelyWithBackendFilter(true)
		, _CanShowRealTimeThumbnails(false)
		, _CanShowDevelopersFolder(false)
		, _CanShowFavorites(false)
		, _CanDockCollections(false)
		, _SelectionMode( ESelectionMode::Multi )
		, _AllowDragging(true)
		, _AllowFocusOnSync(true)
		, _FillEmptySpaceInTileView(true)
		, _ShowPathInColumnView(false)
		, _ShowTypeInColumnView(true)
		, _SortByPathInColumnView(false)
		, _ShowTypeInTileView(true)
		, _ForceShowEngineContent(false)
		, _ForceShowPluginContent(false)
		, _ForceHideScrollbar(false)
		, _ShowDisallowedAssetClassAsUnsupportedItems(false)
		{}

		/** Called to check if an asset should be filtered out by external code */
		SLATE_EVENT( FOnShouldFilterAsset, OnShouldFilterAsset )

		/** Called to check if an item should be filtered out by external code */
		SLATE_EVENT(FOnShouldFilterItem, OnShouldFilterItem)

		/** Called when the asset view is asked to start to create a temporary item */
		SLATE_EVENT( FOnAssetViewNewItemRequested, OnNewItemRequested )

		/** Called to when an item is selected */
		SLATE_EVENT( FOnContentBrowserItemSelectionChanged, OnItemSelectionChanged )

		/** Called when the user double clicks, presses enter, or presses space on an Content Browser item */
		SLATE_EVENT( FOnContentBrowserItemsActivated, OnItemsActivated )

		/** Delegate to invoke when a context menu for an item is opening. */
		SLATE_EVENT( FOnGetContentBrowserItemContextMenu, OnGetItemContextMenu )

		/** Called when the user has committed a rename of one or more items */
		SLATE_EVENT( FOnContentBrowserItemRenameCommitted, OnItemRenameCommitted )

		/** Delegate to call (if bound) to check if it is valid to get a custom tooltip for this asset item */
		SLATE_EVENT(FOnIsAssetValidForCustomToolTip, OnIsAssetValidForCustomToolTip)

		/** Called to get a custom asset item tool tip (if necessary) */
		SLATE_EVENT( FOnGetCustomAssetToolTip, OnGetCustomAssetToolTip )

		/** Called when an asset item is about to show a tooltip */
		SLATE_EVENT( FOnVisualizeAssetToolTip, OnVisualizeAssetToolTip )

		/** Called when an asset item's tooltip is closing */
		SLATE_EVENT(FOnAssetToolTipClosing, OnAssetToolTipClosing)

		/** Called when opening view options menu */
		SLATE_EVENT(FOnExtendAssetViewOptionsMenuContext, OnExtendAssetViewOptionsMenuContext)

		/** Initial set of item categories that this view should show - may be adjusted further by things like CanShowClasses or legacy delegate bindings */
		SLATE_ARGUMENT( EContentBrowserItemCategoryFilter, InitialCategoryFilter )

		/** The warning text to display when there are no assets to show */
		SLATE_ATTRIBUTE( FText, AssetShowWarningText )

		/** Attribute to determine what text should be highlighted */
		SLATE_ATTRIBUTE( FText, HighlightedText )

		/** What the label on the asset thumbnails should be */
		SLATE_ARGUMENT( EThumbnailLabel::Type, ThumbnailLabel )

		/** Whether to ever show the hint label on thumbnails */
		SLATE_ARGUMENT( bool, AllowThumbnailHintLabel )

		/** The filter collection used to further filter down assets returned from the backend */
		SLATE_ARGUMENT( TSharedPtr<FAssetFilterCollectionType>, FrontendFilters )

		/** Show path view filters submenu in view options menu */
		SLATE_ARGUMENT( bool, bShowPathViewFilters )

		/** The initial base sources filter */
		SLATE_ARGUMENT( FSourcesData, InitialSourcesData )

		/** The initial backend filter */
		SLATE_ARGUMENT( FARFilter, InitialBackendFilter )

		/** The asset that should be initially selected */
		SLATE_ARGUMENT( FAssetData, InitialAssetSelection )

		/** The initial view type */
		SLATE_ARGUMENT( EAssetViewType::Type, InitialViewType )

		/** Initial thumbnail size */
		SLATE_ARGUMENT( EThumbnailSize, InitialThumbnailSize )

		/** Should the toolbar indicating number of selected assets, mode switch buttons, etc... be shown? */
		SLATE_ARGUMENT( bool, ShowBottomToolbar )

		/** Should view options be shown. Note: If ShowBottomToolbar is false, then view options are not shown regardless of this setting */
		SLATE_ARGUMENT( bool, ShowViewOptions)

		/** True if the asset view may edit thumbnails */
		SLATE_ARGUMENT( bool, AllowThumbnailEditMode )

		/** Indicates if this view is allowed to show classes */
		SLATE_ARGUMENT( bool, CanShowClasses )

		/** Indicates if the 'Show Folders' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowFolders )

		/** Indicates if this view is allowed to show folders that cannot be written to */
		SLATE_ARGUMENT( bool, CanShowReadOnlyFolders )

		/** If true, recursive filtering will be caused by applying a backend filter */
		SLATE_ARGUMENT( bool, FilterRecursivelyWithBackendFilter )

		/** Indicates if the 'Real-Time Thumbnails' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowRealTimeThumbnails )

		/** Indicates if the 'Show Developers' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowDevelopersFolder )

		/** Indicates if the 'Show Favorites' option should be enabled or disabled */
		SLATE_ARGUMENT(bool, CanShowFavorites)

		/** Indicates if the 'Dock Collections' option should be enabled or disabled */
		SLATE_ARGUMENT(bool, CanDockCollections)

		/** The selection mode the asset view should use */
		SLATE_ARGUMENT( ESelectionMode::Type, SelectionMode )

		/** Whether to allow dragging of items */
		SLATE_ARGUMENT( bool, AllowDragging )

		/** Whether this asset view should allow focus on sync or not */
		SLATE_ARGUMENT( bool, AllowFocusOnSync )

		/** Whether this asset view should allow the thumbnails to consume empty space after the user scale is applied */
		SLATE_ARGUMENT( bool, FillEmptySpaceInTileView )

		/** Should show Path in column view if true */
		SLATE_ARGUMENT(bool, ShowPathInColumnView)

		/** Should show Type in column view if true */
		SLATE_ARGUMENT(bool, ShowTypeInColumnView)

		/** Sort by path in the column view. Only works if the initial view type is Column */
		SLATE_ARGUMENT(bool, SortByPathInColumnView)
		
		/** Should show Type in column view if true */
		SLATE_ARGUMENT(bool, ShowTypeInTileView)

		/** Should always show engine content */
		SLATE_ARGUMENT(bool, ForceShowEngineContent)

		/** Should always show plugin content */
		SLATE_ARGUMENT(bool, ForceShowPluginContent)

		/** Should always hide scrollbar (Removes scrollbar) */
		SLATE_ARGUMENT(bool, ForceHideScrollbar)

		/** Allow the asset view to display the hidden asset class as unsupported items */
		SLATE_ARGUMENT(bool, ShowDisallowedAssetClassAsUnsupportedItems)

		/** Called to check if an asset tag should be display in details view. */
		SLATE_EVENT( FOnShouldDisplayAssetTag, OnAssetTagWantsToBeDisplayed )

		/** Called to add extra asset data to the asset view, to display virtual assets. These get treated similar to Class assets */
		SLATE_EVENT( FOnGetCustomSourceAssets, OnGetCustomSourceAssets )

		/** Columns to hide by default */
		SLATE_ARGUMENT( TArray<FString>, HiddenColumnNames )
			
		/** Custom columns that can be use specific */
		SLATE_ARGUMENT( TArray<FAssetViewCustomColumn>, CustomColumns )

		/** Custom columns that can be use specific */
		SLATE_EVENT( FOnSearchOptionChanged, OnSearchOptionsChanged)

		/** The content browser that owns this view if any */
		SLATE_ARGUMENT(TSharedPtr<SContentBrowser>, OwningContentBrowser)
	SLATE_END_ARGS()

	~SAssetView();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Changes the base sources for this view */
	void SetSourcesData(const FSourcesData& InSourcesData);

	/** Returns the sources filter applied to this asset view */
	const FSourcesData& GetSourcesData() const;

	/** Returns true if a real asset path is selected (i.e \Engine\* or \Game\*) */
	bool IsAssetPathSelected() const;

	/** Notifies the asset view that the filter-list filter has changed */
	void SetBackendFilter(const FARFilter& InBackendFilter);

	/** Get the current backend filter */
	const FARFilter& GetBackendFilter() const { return BackendFilter; }
	
	/** Handler for when a data source requests folder item creation */
	void NewFolderItemRequested(const FContentBrowserItemTemporaryContext& NewItemContext);

	/** Handler for when a data source requests file item creation */
	void NewFileItemRequested(const FContentBrowserItemDataTemporaryContext& NewItemContext);

	/** Creates a new asset item designed to allocate a new object once it is named. Uses the supplied factory to create the asset */
	void CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory);

	/** Sets up an inline rename for the specified item */
	void RenameItem(const FContentBrowserItem& ItemToRename);

	/** Selects the specified items. */
	void SyncToItems( TArrayView<const FContentBrowserItem> ItemsToSync, const bool bFocusOnSync = true );

	/** Selects the specified virtual paths. */
	void SyncToVirtualPaths( TArrayView<const FName> VirtualPathsToSync, const bool bFocusOnSync = true );

	/** Selects the specified assets and paths. */
	void SyncToLegacy( TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bFocusOnSync = true );

	/** Sets the state of the asset view to the one described by the history data */
	void ApplyHistoryData( const FHistoryData& History );

	/** Returns all the items currently selected in the view */
	TArray<TSharedPtr<FAssetViewItem>> GetSelectedViewItems() const;

	/** Returns all the items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedItems() const;

	/** Returns all the folder items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedFolderItems() const;

	/** Returns all the file items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedFileItems() const;

	/** Returns all the asset data objects in items currently selected in the view */
	TArray<FAssetData> GetSelectedAssets() const;

	/** Returns all the folders currently selected in the view */
	TArray<FString> GetSelectedFolders() const;

	/** Requests that the asset view refreshes all it's source items. This is slow and should only be used if the source items change. */
	void RequestSlowFullListRefresh();

	/** Requests that the asset view refreshes only items that are filtered through frontend sources. This should be used when possible. */
	void RequestQuickFrontendListRefresh();

	/** Saves any settings to config that should be persistent between editor sessions */
	void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const;

	/** Loads any settings to config that should be persistent between editor sessions */
	void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString);

	/** Adjusts the selected asset by the selection delta, which should be +1 or -1) */
	void AdjustActiveSelection(int32 SelectionDelta);

	/** Returns true if an asset is currently in the process of being renamed */
	bool IsRenamingAsset() const;

	// SWidget inherited
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent ) override;

	/** Opens the selected assets or folders, depending on the selection */
	void OnOpenAssetsOrFolders();

	/** Loads the selected assets and previews them if possible */
	void OnPreviewAssets();

	/** Clears the selection of all the lists in the view */
	void ClearSelection(bool bForceSilent = false);

	/** Returns true if the asset view is in thumbnail editing mode */
	bool IsThumbnailEditMode() const;

	/** Delegate called when an editor setting is changed */
	void HandleSettingChanged(FName PropertyName);

	/** Set whether the user is currently searching or not */
	void SetUserSearching(bool bInSearching);

	/**
	 * Forces the plugin content folder to be shown.
	 *
	 * @param bEnginePlugin		If true, also forces the engine folder to be shown.
	 */
	void ForceShowPluginFolder( bool bEnginePlugin );

	/** Enables the Show Engine Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowEngineContent();

	/** Enables the Show Developer Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowDeveloperContent();

	/** Enables the Show Plugin Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowPluginContent();

	/** Enables the Show Localized Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowLocalizedContent();

	/** @return true when we are including class names in search criteria */
	bool IsIncludingClassNames() const;

	/** @return true when we are including the entire asset path in search criteria */
	bool IsIncludingAssetPaths() const;

	/** @return true when we are including collection names in search criteria */
	bool IsIncludingCollectionNames() const;

	/** Handler for when the view combo button is clicked */
	TSharedRef<SWidget> GetViewButtonContent();

	/** Sets the view type and updates lists accordingly */
	void SetCurrentViewType(EAssetViewType::Type NewType);

	/** Sets the thumbnail size and updates lists accordingly */
	void SetCurrentThumbnailSize(EThumbnailSize NewThumbnailSize);

	/** Gets the text for the asset count label */
	FText GetAssetCountText() const;

	/** Gets text name for given thumbnail */
	static FText ThumbnailSizeToDisplayName(EThumbnailSize InSize);

	/** Set the filter list attached to this asset view - allows toggling of the the filter bar layout from the view options */
	void SetFilterBar(TSharedPtr<SFilterList> InFilterBar);

private:

	/** Sets the pending selection to the current selection (used when changing views or refreshing the view). */
	void SyncToSelection( const bool bFocusOnSync = true );

	/** @return the thumbnail scale setting path to use when looking up the setting in an ini. */
	FString GetThumbnailScaleSettingPath(const FString& SettingsString) const;

	/** @return the view type setting path to use when looking up the setting in an ini. */
	FString GetCurrentViewTypeSettingPath(const FString& SettingsString) const;

	/** Calculates a new filler scale used to adjust the thumbnails to fill empty space. */
	void CalculateFillScale( const FGeometry& AllottedGeometry );

	/** Calculates the latest color and opacity for the hint on thumbnails */
	void CalculateThumbnailHintColorAndOpacity();

	/** True if we have items pending that ProcessItemsPendingFilter still needs to process */
	bool HasItemsPendingFilter() const;

	/** Handles amortizing the additional filters */
	void ProcessItemsPendingFilter(const double TickStartTime);

	/** Creates a new tile view */
	TSharedRef<SAssetTileView> CreateTileView();

	/** Creates a new list view */
	TSharedRef<SAssetListView> CreateListView();

	/** Creates a new column view */
	TSharedRef<SAssetColumnView> CreateColumnView();

	const FSlateBrush* GetRevisionControlColumnIconBadge() const;

	/** Returns true if the specified search token is allowed */
	bool IsValidSearchToken(const FString& Token) const;

	/** Regenerates the AssetItems list from the AssetRegistry */
	void RefreshSourceItems();

	/** Regenerates the FilteredAssetItems list from the AssetItems list */
	void RefreshFilteredItems();

	/** Sets the asset type that represents the majority of the assets in view */
	void SetMajorityAssetType(FName NewMajorityAssetType);

	/** Handler for when an asset is added to a collection */
	void OnAssetsAddedToCollection( const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths );

	/** Handler for when an asset is removed from a collection */
	void OnAssetsRemovedFromCollection( const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths );

	/** Handler for when a collection is renamed */
	void OnCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection );

	/** Handler for when a collection is updated */
	void OnCollectionUpdated( const FCollectionNameType& Collection );

	/** Handler for when any frontend filters have been changed */
	void OnFrontendFiltersChanged();

	/** Returns true if there is any frontend filter active */
	bool IsFrontendFilterActive() const;

	/** Returns true if the specified Content Browser item passes all applied frontend (non asset registry) filters */
	bool PassesCurrentFrontendFilter(const FContentBrowserItem& Item) const;

	/** Returns true if the current filters deem that the asset view should be filtered recursively (overriding folder view) */
	bool ShouldFilterRecursively() const;

	/** Sorts the contents of the asset view alphabetically */
	void SortList(bool bSyncToSelection = true);

	/** Returns the thumbnails hint color and opacity */
	FLinearColor GetThumbnailHintColorAndOpacity() const;

	/** Register menu for when the view combo button is clicked */
	static void RegisterGetViewButtonMenu();

	/** Fill in menu content for when the view combo button is clicked */
	void PopulateViewButtonMenu(UToolMenu* Menu);

	/** Toggle whether folders should be shown or not */
	void ToggleShowFolders();

	/** Whether or not it's possible to show folders */
	bool IsToggleShowFoldersAllowed() const;

	/** @return true when we are showing folders */
	bool IsShowingFolders() const;

	/** @return true when we are showing read-only folders */
	bool IsShowingReadOnlyFolders() const;

	/** Toggle whether empty folders should be shown or not */
	void ToggleShowEmptyFolders();

	/** Whether or not it's possible to show empty folders */
	bool IsToggleShowEmptyFoldersAllowed() const;

	/** @return true when we are showing empty folders */
	bool IsShowingEmptyFolders() const;

	/** Toggle whether localized content should be shown or not */
	void ToggleShowLocalizedContent();

	/** Whether or not it's possible to show localized content */
	bool IsToggleShowLocalizedContentAllowed() const;

	/** @return true when we are showing folders */
	bool IsShowingLocalizedContent() const;

	/** Toggle whether to show real-time thumbnails */
	void ToggleRealTimeThumbnails();

	/** Whether it is possible to show real-time thumbnails */
	bool CanShowRealTimeThumbnails() const;

	/** @return true if we are showing real-time thumbnails */
	bool IsShowingRealTimeThumbnails() const;

	/** Toggle whether plugin content should be shown or not */
	void ToggleShowPluginContent();

	/** @return true when we are showing plugin content */
	bool IsShowingPluginContent() const;

	/** Toggle whether engine content should be shown or not */
	void ToggleShowEngineContent();

	/** @return true when we are showing engine content */
	bool IsShowingEngineContent() const;

	/** Toggle whether developers content should be shown or not */
	void ToggleShowDevelopersContent();

	/** Whether or not it's possible to toggle developers content */
	bool IsToggleShowDevelopersContentAllowed() const;

	/** Whether or not it's possible to toggle engine content */
	bool IsToggleShowEngineContentAllowed() const;

	/** Whether or not it's possible to toggle plugin content */
	bool IsToggleShowPluginContentAllowed() const;
	
	/** @return true when we are showing the developers content */
	bool IsShowingDevelopersContent() const;

	/** Toggle whether favorites should be shown or not */
	void ToggleShowFavorites();

	/** Whether or not it's possible to toggle favorites */
	bool IsToggleShowFavoritesAllowed() const;

	/** @return true when we are showing favorites */
	bool IsShowingFavorites() const;

	/** Toggle whether the collections view should be docked under the paths view */
	void ToggleDockCollections();

	/** Whether or not it's possible to dock the collections view */
	bool IsToggleDockCollectionsAllowed() const;

	/** @return true when the collections view is docked */
	bool HasDockedCollections() const;

	/** Toggle whether C++ content should be shown or not */
	void ToggleShowCppContent();

	/** Whether or not it's possible to show C++ content */
	bool IsToggleShowCppContentAllowed() const;

	/** @return true when we are showing C++ content */
	bool IsShowingCppContent() const;

	/** Toggle whether class names should be included in search criteria */
	void ToggleIncludeClassNames();

	/** Whether or not it's possible to include class names in search criteria */
	bool IsToggleIncludeClassNamesAllowed() const;

	/** Toggle whether the entire asset path should be included in search criteria */
	void ToggleIncludeAssetPaths();

	/** Whether or not it's possible to include the entire asset path in search criteria */
	bool IsToggleIncludeAssetPathsAllowed() const;

	/** Toggle whether collection names should be included in search criteria */
	void ToggleIncludeCollectionNames();

	/** Whether or not it's possible to include collection names in search criteria */
	bool IsToggleIncludeCollectionNamesAllowed() const;

	/** @return true when we are filtering recursively when we have an asset path */
	bool IsFilteringRecursively() const;

	/** Whether or not it's possible to toggle how to filtering recursively */
	bool IsToggleFilteringRecursivelyAllowed() const;

	/** Toggle whether we're filtering recursively */
	void ToggleFilteringRecursively();

	/** Toggle whether we are showing the all virtual folder */
	void ToggleShowAllFolder();

	/** @return true when we are showing the all virtual folder */
	bool IsShowingAllFolder() const;

	/** Toggle whether we are re-organizing folders into virtual locations */
	void ToggleOrganizeFolders();

	/** @return true when we are organizing folders into virtual locations */
	bool IsOrganizingFolders() const;

	/** Sets the view type and forcibly dismisses all currently open context menus */
	void SetCurrentViewTypeFromMenu(EAssetViewType::Type NewType);

	/** Clears the reference to the current view and creates a new one, based on CurrentViewType */
	void CreateCurrentView();

	/** Gets the current view type (list or tile) */
	EAssetViewType::Type GetCurrentViewType() const;

	TSharedRef<SWidget> CreateShadowOverlay( TSharedRef<STableViewBase> Table );

	/** Returns true if ViewType is the current view type */
	bool IsCurrentViewType(EAssetViewType::Type ViewType) const;

	/** Set the keyboard focus to the correct list view that should be active */
	void FocusList() const;

	/** Refreshes the list view to display any changes made to the non-filtered assets */
	void RefreshList();

	/** Sets the sole selection for all lists in the view */
	void SetSelection(const TSharedPtr<FAssetViewItem>& Item);

	/** Sets selection for an item in all lists in the view */
	void SetItemSelection(const TSharedPtr<FAssetViewItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Scrolls the selected item into view for all lists in the view */
	void RequestScrollIntoView(const TSharedPtr<FAssetViewItem>& Item);

	/** Handler for list view widget creation */
	TSharedRef<ITableRow> MakeListViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for tile view widget creation */
	TSharedRef<ITableRow> MakeTileViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for column view widget creation */
	TSharedRef<ITableRow> MakeColumnViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for when any asset item widget gets destroyed */
	void AssetItemWidgetDestroyed(const TSharedPtr<FAssetViewItem>& Item);
	
	/** Creates new thumbnails that are near the view area and deletes old thumbnails that are no longer relevant. */
	void UpdateThumbnails();

	/**  Helper function for UpdateThumbnails. Adds the specified item to the new thumbnail relevancy map and creates any thumbnails for new items. Returns the thumbnail. */
	TSharedPtr<FAssetThumbnail> AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FAssetViewItem>& Item, TMap< TSharedPtr<FAssetViewItem>, TSharedPtr<FAssetThumbnail> >& NewRelevantThumbnails);

	/** Handler for tree view selection changes */
	void AssetSelectionChanged( TSharedPtr<FAssetViewItem > AssetItem, ESelectInfo::Type SelectInfo );

	/** Handler for when an item has scrolled into view after having been requested to do so */
	void ItemScrolledIntoView(TSharedPtr<FAssetViewItem> AssetItem, const TSharedPtr<ITableRow>& Widget);

	/** Handler for context menus */
	TSharedPtr<SWidget> OnGetContextMenuContent();

	/** Handler called when an asset context menu is about to open */
	bool CanOpenContextMenu() const;

	/** Handler for double clicking an item */
	void OnListMouseButtonDoubleClick(TSharedPtr<FAssetViewItem> AssetItem);

	/** Handle dragging an asset */
	FReply OnDraggingAssetItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Checks if the asset name being committed is valid */
	bool AssetVerifyRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FText& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage);

	/** An asset item has started to be renamed */
	void AssetRenameBegin(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor);

	/** An asset item that was prompting the user for a new name was committed. Return false to indicate that the user should enter a new name */
	void AssetRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor, const ETextCommit::Type CommitType);

	/** Gets the color and opacity for all names of assets in the asset view */
	FLinearColor GetAssetNameColorAndOpacity() const;

	/** Returns true if tooltips should be allowed right now. Tooltips are typically disabled while right click scrolling. */
	bool ShouldAllowToolTips() const;

	/** Returns true if the asset view is currently allowing the user to edit thumbnails */
	bool IsThumbnailEditModeAllowed() const;

	/** The "Done Editing" button was pressed in the thumbnail edit mode strip */
	FReply EndThumbnailEditModeClicked();

	/** Gets the visibility of the Thumbnail Edit Mode label */
	EVisibility GetEditModeLabelVisibility() const;

	/** Gets the visibility of the list view */
	EVisibility GetListViewVisibility() const;

	/** Gets the visibility of the tile view */
	EVisibility GetTileViewVisibility() const;

	/** Gets the visibility of the column view */
	EVisibility GetColumnViewVisibility() const;

	/** Toggles thumbnail editing mode */
	void ToggleThumbnailEditMode();

	/** Called when  thumbnail size is changed */
	void OnThumbnailSizeChanged(EThumbnailSize NewThumbnailSize);

	/** Is the current thumbnail size menu option checked */
	bool IsThumbnailSizeChecked(EThumbnailSize InThumbnailSize) const;

	/** Are thumbnails allowed to be scaled by the user */
	bool IsThumbnailScalingAllowed() const;

	/** Gets the current thumbnail scale */
	float GetThumbnailScale() const;

	/** Gets the current thumbnail size enum */
	EThumbnailSize GetThumbnailSize() const { return ThumbnailSize; }

	/** Gets the spacing dedicated to support type names */
	float GetTileViewTypeNameHeight() const;

	/** Gets the spacing needed to support source control icons. No scaling is required for this since the icon itself is not scaled by the thumbnail size. */
	float GetSourceControlIconHeight() const;

	/** Gets the scaled item height for the list view */
	float GetListViewItemHeight() const;
	
	/** Gets the final scaled item height for the tile view */
	float GetTileViewItemHeight() const;

	/** Gets the scaled item height for the tile view before the filler scale is applied */
	float GetTileViewItemBaseHeight() const;

	/** Gets the final scaled item width for the tile view */
	float GetTileViewItemWidth() const;

	/** Gets the scaled item width for the tile view before the filler scale is applied */
	float GetTileViewItemBaseWidth() const;

	/** Gets the sort mode for the supplied ColumnId */
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	/** Gets the sort order for the supplied ColumnId */
	EColumnSortPriority::Type GetColumnSortPriority(const FName ColumnId) const;

	/** Handler for when a column header is clicked */
	void OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);

	/** @return The state of the is working progress bar */
	TOptional< float > GetIsWorkingProgressBarState() const;

	/** Is the no assets to show warning visible? */
	EVisibility IsAssetShowWarningTextVisible() const;

	/** Gets the text for displaying no assets to show warning */
	FText GetAssetShowWarningText() const;

	/** Whether we have a single source collection selected */
	bool HasSingleCollectionSource() const;

	/** Starts the async flow of creating a file or folder item from deferred data */
	void BeginCreateDeferredItem();

	/** Finishes the async flow of creating a file or folder item from deferred data */
	FContentBrowserItem EndCreateDeferredItem(const TSharedPtr<FAssetViewItem>& InItem, const FString& InName, const bool bFinalize, FText& OutErrorText);

	/** @return The current quick-jump term */
	FText GetQuickJumpTerm() const;

	/** @return Whether the quick-jump term is currently visible? */
	EVisibility IsQuickJumpVisible() const;

	/** @return The color that should be used for the quick-jump term */
	FSlateColor GetQuickJumpColor() const;

	/** Reset the quick-jump to its empty state */
	void ResetQuickJump();

	/**
	 * Called from OnKeyChar and OnKeyDown to handle quick-jump key presses
	 * @param InCharacter		The character that was typed
	 * @param bIsControlDown	Was the control key pressed?
	 * @param bIsAltDown		Was the alt key pressed?
	 * @param bTestOnly			True if we only want to test whether the key press would be handled, but not actually update the quick-jump term
	 * @return FReply::Handled or FReply::Unhandled
	 */
	FReply HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly);

	/**
	 * Perform a quick-jump to the next available asset in FilteredAssetItems that matches the current term
	 * @param bWasJumping		True if we were performing an ongoing quick-jump last Tick
	 * @return True if the quick-jump found a valid match, false otherwise
	 */
	bool PerformQuickJump(const bool bWasJumping);
	
	/** Resets the column filtering state to make them all visible */
	void ResetColumns();

	/** Export columns to CSV */
	void ExportColumns();

	/** Called when a column is shown/hidden in the column view */
	void OnHiddenColumnsChanged();

	/** Will compute the max row size from all its children for the specified column id*/
	FVector2D GetMaxRowSizeForColumn(const FName& ColumnId);

	/** Append the current effective backend filter (intersection of BackendFilter and SupportedFilter) to the given filter. */
	void AppendBackendFilter(FARFilter& FilterToAppendTo) const;

	FContentBrowserDataFilter CreateBackendDataFilter(bool bInvalidateCache) const;

	/** Handles updating the view when content items are changed */
	void HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems);

	/** Notification for when the content browser has completed it's initial search */
	void HandleItemDataDiscoveryComplete();

	/** Get the config struct for the owning Content Browser, if one exists. */
	FContentBrowserInstanceConfig* GetContentBrowserConfig() const;

	/** Get the config struct for this asset view, if one exists. */
	FAssetViewInstanceConfig* GetAssetViewConfig() const;

private:
	friend class FAssetViewFrontendFilterHelper;

	/** The available items from querying the backend data sources */
	TMap<FContentBrowserItemKey, TSharedPtr<FAssetViewItem>> AvailableBackendItems;

	/**
	 * The items from AvailableBackendItems that are pending a run through any additional filtering before they can be shown in the filtered view list.
	 * @note This filtering will run without amortization via ProcessItemsPendingFilter, so only use it for items that *must* be processed this frame.
	 */
	TSet<TSharedPtr<FAssetViewItem>> ItemsPendingPriorityFilter;

	/**
	 * The items from AvailableBackendItems that are pending a run through any additional frontend filters before they can be shown in the filtered view list.
	 * @note This filtering will run amortized on the game thread via ProcessItemsPendingFilter.
	 */
	TSet<TSharedPtr<FAssetViewItem>> ItemsPendingFrontendFilter;

	/** The items that are being shown in the filtered view list */
	TArray<TSharedPtr<FAssetViewItem>> FilteredAssetItems;

	/* Map of an item name to the current count of FilteredAssetItems that are of that type */ 
	TMap<FName, int32> FilteredAssetItemTypeCounts;

	/** The list view that is displaying the assets */
	EAssetViewType::Type CurrentViewType;
	TSharedPtr<SAssetListView> ListView;
	TSharedPtr<SAssetTileView> TileView;
	TSharedPtr<SAssetColumnView> ColumnView;
	TSharedPtr<SBox> ViewContainer;

	/** The content browser that created this asset view if any */
	TWeakPtr<SContentBrowser> OwningContentBrowser;

	/** The Filter Bar attached to this asset view if any */
	TWeakPtr<SFilterList> FilterBar;

	/** The current base source filter for the view */
	FSourcesData SourcesData;
	FARFilter BackendFilter;
	TSharedPtr<FPathPermissionList> AssetClassPermissionList;
	TSharedPtr<FPathPermissionList> FolderPermissionList;
	TSharedPtr<FPathPermissionList> WritableFolderPermissionList;
	TSharedPtr<FAssetFilterCollectionType> FrontendFilters;

	/** Show path view filters submenu in view options menu  */
	bool bShowPathViewFilters;

	/** If true, the source items will be refreshed next frame. Very slow. */
	bool bSlowFullListRefreshRequested;

	/** If true, the frontend items will be refreshed next frame. Much faster. */
	bool bQuickFrontendListRefreshRequested;

	/** The list of items to sync next frame */
	FSelectionData PendingSyncItems;

	/** Should we take focus when the PendingSyncAssets are processed? */
	bool bPendingFocusOnSync;

	/** Was recursive filtering enabled the last time a full slow refresh performed? */
	bool bWereItemsRecursivelyFiltered;

	/** Called to check if an asset should be filtered out by external code */
	FOnShouldFilterAsset OnShouldFilterAsset;

	/** Called to check if an item should be filtered out by external code */
	FOnShouldFilterItem OnShouldFilterItem;

	/** Called when the asset view is asked to start to create a temporary item */
	FOnAssetViewNewItemRequested OnNewItemRequested;

	/** Called when an item was selected in the list. Provides more context than OnAssetSelected. */
	FOnContentBrowserItemSelectionChanged OnItemSelectionChanged;

	/** Called when the user double clicks, presses enter, or presses space on a Content Browser item */
	FOnContentBrowserItemsActivated OnItemsActivated;

	/** Delegate to invoke when generating the context menu for an item */
	FOnGetContentBrowserItemContextMenu OnGetItemContextMenu;

	/** Called when the user has committed a rename of one or more items */
	FOnContentBrowserItemRenameCommitted OnItemRenameCommitted;

	/** Called to check if an asset tag should be display in details view. */
	FOnShouldDisplayAssetTag OnAssetTagWantsToBeDisplayed;

	/** Called to see if it is valid to get a custom asset tool tip */
	FOnIsAssetValidForCustomToolTip OnIsAssetValidForCustomToolTip;

	/** Called to get a custom asset item tooltip (If necessary) */
	FOnGetCustomAssetToolTip OnGetCustomAssetToolTip;

	/** Called when a custom asset item is about to show a tooltip */
	FOnVisualizeAssetToolTip OnVisualizeAssetToolTip;

	/** Called when a custom asset item's tooltip is closing */
	FOnAssetToolTipClosing OnAssetToolTipClosing;

	/** Called to add extra asset data to the asset view, to display virtual assets. These get treated similar to Class assets */
	FOnGetCustomSourceAssets OnGetCustomSourceAssets;

	/** Called when a search option changes to notify that results should be rebuilt */
	FOnSearchOptionChanged OnSearchOptionsChanged;

	/** Called when opening view options menu */
	FOnExtendAssetViewOptionsMenuContext OnExtendAssetViewOptionsMenuContext;

	/** When true, filtered list items will be sorted next tick. Provided another sort hasn't happened recently or we are renaming an asset */
	bool bPendingSortFilteredItems;
	double CurrentTime;
	double LastSortTime;
	double SortDelaySeconds;

	/** Weak ptr to the asset that is waiting to be renamed when scrolled into view, and the window is active */
	TWeakPtr<FAssetViewItem> AwaitingRename;

	/** Set when the user is in the process of naming an asset */
	TWeakPtr<FAssetViewItem> RenamingAsset;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** A map of FAssetViewAsset to the thumbnail that represents it. Only items that are currently visible or within half of the FilteredAssetItems array index distance described by NumOffscreenThumbnails are in this list */
	TMap< TSharedPtr<FAssetViewItem>, TSharedPtr<FAssetThumbnail> > RelevantThumbnails;

	/** The set of FAssetItems that currently have widgets displaying them. */
	TArray< TSharedPtr<FAssetViewItem> > VisibleItems;

	/** The number of thumbnails to keep for asset items that are not currently visible. Half of the thumbnails will be before the earliest item and half will be after the latest. */
	int32 NumOffscreenThumbnails;

	/** The current size of relevant thumbnails */
	int32 CurrentThumbnailSize;

	/** Flag to defer thumbnail updates until the next frame */
	bool bPendingUpdateThumbnails;

	/** The size of thumbnails */
	int32 ListViewThumbnailResolution;
	int32 ListViewThumbnailSize;
	int32 ListViewThumbnailPadding;
	int32 TileViewThumbnailResolution;
	int32 TileViewThumbnailSize;
	int32 TileViewThumbnailPadding;
	int32 TileViewNameHeight;

	/** The max and min thumbnail scales as a fraction of the rendered size */
	float MinThumbnailScale;
	float MaxThumbnailScale;

	/** Scalar applied to thumbnail sizes so that users thumbnails are scaled based on users display area size*/
	float ThumbnailScaleRangeScalar;

	/** Current thumbnail size */
	EThumbnailSize ThumbnailSize;

	/** The amount to scale each thumbnail so that the empty space is filled. */
	float FillScale;

	/** When in columns view, this is the name of the asset type which is most commonly found in the recent results */
	FName MajorityAssetType;

	/** The manager responsible for sorting assets in the view */
	FAssetViewSortManager SortManager;

	/** Flag indicating if we will be filling the empty space in the tile view. */
	bool bFillEmptySpaceInTileView;

	/** When true, selection change notifications will not be sent */
	bool bBulkSelecting;

	/** When true, the user may edit thumbnails */
	bool bAllowThumbnailEditMode : 1;

	/** True when the asset view is currently allowing the user to edit thumbnails */
	bool bThumbnailEditMode : 1;

	/** Indicates if this view is allowed to show classes */
	bool bCanShowClasses : 1;

	/** Indicates if the 'Show Folders' option should be enabled or disabled */
	bool bCanShowFolders : 1;

	/** Indicates if this view is allowed to show folders that cannot be written to */
	bool bCanShowReadOnlyFolders : 1;

	/** If true, recursive filtering will be caused by applying a backend filter */
	bool bFilterRecursivelyWithBackendFilter : 1;

	/** Indicates if the 'Real-Time Thumbnails' option should be enabled or disabled */
	bool bCanShowRealTimeThumbnails : 1;

	/** Indicates if the 'Show Developers' option should be enabled or disabled */
	bool bCanShowDevelopersFolder : 1;

	/** Indicates if the 'Show Favorites' option should be enabled or disabled */
	bool bCanShowFavorites : 1;

	/** Indicates if the 'Dock Collections' option should be enabled or disabled */
	bool bCanDockCollections : 1;

	/** If true, it will show path column in the asset view */
	bool bShowPathInColumnView : 1;

	/** If true, it will show type in the asset view */
	bool bShowTypeInColumnView : 1;

	/** If true, it sorts by path and then name */
	bool bSortByPathInColumnView : 1;

	/** If true, it will show type in the tile view */
	bool bShowTypeInTileView : 1;

	/** If true, engine content is always shown */
	bool bForceShowEngineContent : 1;

	/** If true, plugin content is always shown */
	bool bForceShowPluginContent : 1;

	/** If true, scrollbar is never shown, removes scroll border entirely */
	bool bForceHideScrollbar : 1;

	/** Whether to allow dragging of items */
	bool bAllowDragging : 1;

	/** Whether this asset view should allow focus on sync or not */
	bool bAllowFocusOnSync : 1;

	/** Flag set if the user is currently searching */
	bool bUserSearching : 1;
	
	/** Whether or not to notify about newly selected items on on the next asset sync */
	bool bShouldNotifyNextAssetSync : 1;

	/** The current selection mode used by the asset view */
	ESelectionMode::Type SelectionMode;

	/** The max number of results to process per tick */
	float MaxSecondsPerFrame;

	/** When delegate amortization began */
	double AmortizeStartTime;

	/** The total time spent amortizing the delegate filter */
	double TotalAmortizeTime;

	/** The initial number of tasks when we started performing amortized work (used to display a progress cue to the user) */
	int32 InitialNumAmortizedTasks;

	/** The text to highlight on the assets */
	TAttribute< FText > HighlightedText;

	/** What the label on the thumbnails should be */
	EThumbnailLabel::Type ThumbnailLabel;

	/** Whether to ever show the hint label on thumbnails */
	bool AllowThumbnailHintLabel;

	/** The sequence used to generate the opacity of the thumbnail hint */
	FCurveSequence ThumbnailHintFadeInSequence;

	/** The current thumbnail hint color and opacity*/
	FLinearColor ThumbnailHintColorAndOpacity;

	/** The text to show when there are no assets to show */
	TAttribute< FText > AssetShowWarningText;

	/** Initial set of item categories that this view should show - may be adjusted further by things like CanShowClasses or legacy delegate bindings */
	EContentBrowserItemCategoryFilter InitialCategoryFilter;

	bool bShowDisallowedAssetClassAsUnsupportedItems = false;

	/** A struct to hold data for the deferred creation of a file or folder item */
	struct FCreateDeferredItemData
	{
		FContentBrowserItemTemporaryContext ItemContext;

		bool bWasAddedToView = false;
	};

	/** File or folder item pending deferred creation */
	TUniquePtr<FCreateDeferredItemData> DeferredItemToCreate;

	/** Struct holding the data for the asset quick-jump */
	struct FQuickJumpData
	{
		FQuickJumpData()
			: bIsJumping(false)
			, bHasChangedSinceLastTick(false)
			, bHasValidMatch(false)
			, LastJumpTime(0)
		{
		}

		/** True if we're currently performing an ongoing quick-jump */
		bool bIsJumping;

		/** True if the jump data has changed since the last Tick */
		bool bHasChangedSinceLastTick;

		/** True if the jump term found a valid match */
		bool bHasValidMatch;

		/** Time (taken from Tick) that we last performed a quick-jump */
		double LastJumpTime;

		/** The string we should be be looking for */
		FString JumpTerm;
	};

	/** Data for the asset quick-jump */
	FQuickJumpData QuickJumpData;
	
	/** Column filtering state */
	TArray<FString> DefaultHiddenColumnNames;
	TArray<FString> HiddenColumnNames;

	TArray<FAssetViewCustomColumn> CustomColumns;

	/** An Id for the cache of the data sources for the filters compilation */
	FContentBrowserDataFilterCacheIDOwner FilterCacheID;
	
	/*
	 * Telemetry-related fields
	 */ 
	// Guid to correlate different telemetry events for this instance of an asset view over time 
	FGuid ViewCorrelationGuid; 
	// Guid to correlate different telemetry events for a 'session' of search/filtering, reset when new backend items are gathered for filtering
	FGuid FilterSessionCorrelationGuid;
	// In progress filter session telemetry - will be sent when filtering ends or user cancels or interacts with a partial filter set
	UE::Telemetry::ContentBrowser::FFrontendFilterTelemetry CurrentFrontendFilterTelemetry;
	
	// If there is in-progress filter telemetry, mark it as complete and send pending data 
	void OnCompleteFiltering(double InAmortizeDuration);
	// If there is in-progress filter telemetry, mark it as incomplete and send pending data 
	void OnInterruptFiltering();
	// Update telemetry for user interaction during an incomplete filtering
	void OnInteractDuringFiltering();

public:
	bool ShouldColumnGenerateWidget(const FString ColumnName) const;
};
