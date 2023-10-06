// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetViewUtils.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserItem.h"
#include "Delegates/Delegate.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IContentBrowserSingleton.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Misc/TextFilter.h"
#include "PathViewTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateConstants.h"
#include "Types/SlateEnums.h"
#include "Types/SlateVector2.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class FContentBrowserItemData;
class FContentBrowserItemDataUpdate;
class FContentBrowserPluginFilter;
class FPathPermissionList;
class FSourcesSearch;
class ITableRow;
class SWidget;
class UToolMenu;
struct FAssetData;
struct FGeometry;
struct FHistoryData;
struct FPathViewConfig;
struct FPointerEvent;
struct FContentBrowserInstanceConfig;

typedef TTextFilter< const FString& > FolderTextFilter;

/**
 * The tree view of folders which contain content.
 */
class SPathView : public SCompoundWidget
{
public:
	/** Delegate for when plugin filters have changed */
	DECLARE_DELEGATE( FOnFrontendPluginFilterChanged );

	SLATE_BEGIN_ARGS( SPathView )
		: _InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAll)
		, _FocusSearchBoxWhenOpened(true)
		, _ShowTreeTitle(false)
		, _SearchBarVisibility(EVisibility::Visible)
		, _ShowSeparator(true)
		, _AllowContextMenu(true)
		, _AllowClassesFolder(false)
		, _AllowReadOnlyFolders(true)
		, _ShowFavorites(false)
		, _SelectionMode( ESelectionMode::Multi )
		{}

		/** Content displayed to the left of the search bar */
		SLATE_NAMED_SLOT( FArguments, SearchContent )

		/** Called when a tree paths was selected */
		SLATE_EVENT( FOnContentBrowserItemSelectionChanged, OnItemSelectionChanged )

		/** Called when a context menu is opening on a item */
		SLATE_EVENT( FOnGetContentBrowserItemContextMenu, OnGetItemContextMenu )

		/** Initial set of item categories that this view should show - may be adjusted further by things like AllowClassesFolder */
		SLATE_ARGUMENT( EContentBrowserItemCategoryFilter, InitialCategoryFilter )

		/** If true, the search box will be focus the frame after construction */
		SLATE_ARGUMENT( bool, FocusSearchBoxWhenOpened )

		/** If true, The tree title will be displayed */
		SLATE_ARGUMENT( bool, ShowTreeTitle )

		/** If EVisibility::Visible, The tree search bar will be displayed */
		SLATE_ATTRIBUTE( EVisibility, SearchBarVisibility )

		/** If true, The tree search bar separator be displayed */
		SLATE_ARGUMENT( bool, ShowSeparator )

		/** If false, the context menu will be suppressed */
		SLATE_ARGUMENT( bool, AllowContextMenu )

		/** If false, the classes folder will be suppressed */
		SLATE_ARGUMENT( bool, AllowClassesFolder )

		/** If true, read only folders will be displayed */
		SLATE_ARGUMENT( bool, AllowReadOnlyFolders )

		/** If true, the favorites expander will be displayed */
		SLATE_ARGUMENT(bool, ShowFavorites);

		/** The selection mode for the tree view */
		SLATE_ARGUMENT( ESelectionMode::Type, SelectionMode )

		/** Optional external search. Will hide and replace our internal search UI */
		SLATE_ARGUMENT( TSharedPtr<FSourcesSearch>, ExternalSearch )

		/** Optional Custom Folder permission list to be used to filter folders. */
		SLATE_ARGUMENT( TSharedPtr<FPathPermissionList>, CustomFolderPermissionList)

		/** The plugin filter collection */
		SLATE_ARGUMENT( TSharedPtr<FPluginFilterCollectionType>, PluginPathFilters)

		/** The instance name of the owning content browser. */
		SLATE_ARGUMENT( FName, OwningContentBrowserName )

	SLATE_END_ARGS()

	/** Destructor */
	~SPathView();

	/** Constructs this widget with InArgs */
	virtual void Construct( const FArguments& InArgs );

	/** Selects the closest matches to the supplied paths in the tree. "/" delimited */
	void SetSelectedPaths(const TArray<FName>& Paths);

	/** Selects the closest matches to the supplied paths in the tree. "/" delimited */
	void SetSelectedPaths(const TArray<FString>& Paths);

	/** Clears selection of all paths */
	void ClearSelection();

	/** Returns the first selected path in the tree view */
	FString GetSelectedPath() const;

	/** Returns all selected paths in the tree view */
	TArray<FString> GetSelectedPaths() const;

	/** Returns all the folder items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedFolderItems() const;

	/** Called when "new folder" is selected in the context menu */
	void NewFolderItemRequested(const FContentBrowserItemTemporaryContext& NewItemContext);

	/** Adds nodes to the tree in order to construct the specified item. If bUserNamed is true, the user will name the folder and the item includes the default name. */
	virtual TSharedPtr<FTreeItem> AddFolderItem(FContentBrowserItemData&& InItem, const bool bUserNamed = false, TArray<TSharedPtr<FTreeItem>>* OutItemsCreated = nullptr);

	/** Attempts to remove the item from the tree. Returns true when successful. */
	bool RemoveFolderItem(const FContentBrowserItemData& InItem);

	/** Sets up an inline rename for the specified folder */
	void RenameFolderItem(const FContentBrowserItem& InItem);

	/**
	 * Selects the paths containing or corresponding to the specified items.
	 *
	 *	@param ItemsToSync			- A list of items to sync the view to
	 *
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset
	 */
	void SyncToItems( TArrayView<const FContentBrowserItem> ItemsToSync, const bool bAllowImplicitSync = false );

	/**
	 * Selects the given virtual paths.
	 *
	 *	@param VirtualPathsToSync	- A list of virtual paths to sync the view to
	 *
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset
	 */
	void SyncToVirtualPaths( TArrayView<const FName> VirtualPathsToSync, const bool bAllowImplicitSync = false );

	/**
	 * Selects the paths containing the specified assets and paths.
	 *
	 *	@param AssetDataList		- A list of assets to sync the view to
	 *	@param FolderList			- A list of folders to sync the view to
	 *
	 *	@param bAllowImplicitSync	- true to allow the view to sync to parent folders if they are already selected,
	 *								  false to force the view to select the explicit Parent folders of each asset
	 */
	void SyncToLegacy( TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bAllowImplicitSync = false );

	/** Finds the item that represents the specified path, if it exists. */
	TSharedPtr<FTreeItem> FindTreeItem(FName InPath) const;

	/** Sets the state of the path view to the one described by the history data */
	void ApplyHistoryData( const FHistoryData& History );

	/** Saves any settings to config that should be persistent between editor sessions */
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& InstanceName) const;

	/** Loads any settings to config that should be persistent between editor sessions */
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& InstanceName);

	/**
	 * Return true if passes path block lists
	 * 
	 *	@param InInternalPath			- Internal Path (e.g. /Game)
	 *	@param InAlreadyCheckedDepth	- Folder depth that has already been checked, 0 if no parts of path already checked
	*/
	bool InternalPathPassesBlockLists(const FStringView InInternalPath, const int32 InAlreadyCheckedDepth = 0) const;

	/** Populates the tree with all folders that are not filtered out */
	virtual void Populate(const bool bIsRefreshingFilter = false);

	/** Sets an alternate tree title*/
	void SetTreeTitle(FText InTitle)
	{
		TreeTitle = InTitle;
	};

	FText GetTreeTitle() const
	{
		return TreeTitle;
	}

	void PopulatePathViewFiltersMenu(UToolMenu* Menu);

	/** Get paths to select by default */
	TArray<FName> GetDefaultPathsToSelect() const;

	/** Get list of root path item names */
	TArray<FName> GetRootPathItemNames() const;	

	/** Get current item category filter enum */
	EContentBrowserItemCategoryFilter GetContentBrowserItemCategoryFilter() const;

	/** Get current item attribute filter enum */
	EContentBrowserItemAttributeFilter GetContentBrowserItemAttributeFilter() const;

protected:
	/** Expands all parents of the specified item */
	void RecursiveExpandParents(const TSharedPtr<FTreeItem>& Item);

	/** Sort the root items into the correct order */
	void SortRootItems();

	/** Handles updating the view when content items are changed */
	virtual void HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems);

	/** Handles updating the view when content items are refreshed */
	void HandleItemDataRefreshed();

	/** Notification for when the content browser has completed it's initial search */
	void HandleItemDataDiscoveryComplete();

	/** Query to see whether the given path is currently filtered from the view */
	virtual bool PathIsFilteredFromViewBySearch(const FString& InPath) const;

	/** Creates a list item for the tree view */
	virtual TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handles focusing a folder widget after it has been created with the intent to rename */
	void TreeItemScrolledIntoView(TSharedPtr<FTreeItem> TreeItem, const TSharedPtr<ITableRow>& Widget);

	/** Handler for tree view selection changes */
	void TreeSelectionChanged(TSharedPtr< FTreeItem > TreeItem, ESelectInfo::Type SelectInfo);

	/** Gets the content for a context menu */
	TSharedPtr<SWidget> MakePathViewContextMenu();

	/** Handler for returning a list of children associated with a particular tree node */
	void GetChildrenForTree(TSharedPtr< FTreeItem > TreeItem, TArray< TSharedPtr<FTreeItem> >& OutChildren);

	/** Handler for when a name was given to a new folder */
	void FolderNameChanged(const TSharedPtr< FTreeItem >& TreeItem, const FString& ProposedName, const UE::Slate::FDeprecateVector2DParameter& MessageLocation, const ETextCommit::Type CommitType);

	/** Handler used to verify the name of a new folder */
	bool VerifyFolderNameChanged(const TSharedPtr< FTreeItem >& TreeItem, const FString& ProposedName, FText& OutErrorMessage) const;

	/** Set the active filter text */
	void SetSearchFilterText(const FText& InSearchText, TArray<FText>& OutErrors);

	/** Gets the string to highlight in tree items (used in folder searching) */
	FText GetHighlightText() const;

	/** True if the specified item is selected in the asset tree */
	bool IsTreeItemSelected(TSharedPtr<FTreeItem> TreeItem) const;

	/** Handler for tree view folders are dragged */
	FReply OnFolderDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	FContentBrowserDataCompiledFilter CreateCompiledFolderFilter() const;

	/** Clear all root items and clear selection */
	void ClearTreeItems();

	/** Get this path view's editor config if OwningContentBrowserName is set. */
	FPathViewConfig* GetPathViewConfig() const;

	/** Get this path view's content browser instance config if OwningContentBrowserName is set. */
	FContentBrowserInstanceConfig* GetContentBrowserConfig() const;

private:
	/** Selects the given path only if it exists. Returns true if selected. */
	bool ExplicitlyAddPathToSelection(const FName Path);

	/** Returns true if the selection changed delegate should be allowed */
	bool ShouldAllowTreeItemChangedDelegate() const;

	/** Handler for recursively expanding/collapsing items in the tree view */
	void SetTreeItemExpansionRecursive( TSharedPtr< FTreeItem > TreeItem, bool bInExpansionState );

	/** Handler for tree view expansion changes */
	void TreeExpansionChanged( TSharedPtr< FTreeItem > TreeItem, bool bIsExpanded );

	/** Handler for when the search box filter has changed */
	void FilterUpdated();

	/** Populates OutSearchStrings with the strings that should be used in searching */
	void PopulateFolderSearchStrings( const FString& FolderName, OUT TArray< FString >& OutSearchStrings ) const;

	/** Returns true if the supplied folder item already exists in the tree. If so, ExistingItem will be set to the found item. */
	bool FolderAlreadyExists(const TSharedPtr< FTreeItem >& TreeItem, TSharedPtr< FTreeItem >& ExistingItem);

	/** Removes the supplied folder from the tree. */
	void RemoveFolderItem(const TSharedPtr< FTreeItem >& TreeItem);

	/** True if the specified item is expanded in the asset tree */
	bool IsTreeItemExpanded(TSharedPtr<FTreeItem> TreeItem) const;

	/** Delegate called when an editor setting is changed */
	void HandleSettingChanged(FName PropertyName);

	/** One-off active timer to focus the widget post-construct */
	EActiveTimerReturnType SetFocusPostConstruct(double InCurrentTime, float InDeltaTime);

	/** One-off active timer to repopulate the path view */
	EActiveTimerReturnType TriggerRepopulate(double InCurrentTime, float InDeltaTime);

	/** Sets the active state of a filter. */
	void SetPluginPathFilterActive(const TSharedRef<FContentBrowserPluginFilter>& Filter, bool bActive);

	/** Unchecks all plugin filters. */
	void ResetPluginPathFilters();

	/** Toggle plugin filter. */
	void PluginPathFilterClicked(TSharedRef<FContentBrowserPluginFilter> Filter);

	/** Returns true if filter is being used. */
	bool IsPluginPathFilterInUse(TSharedRef<FContentBrowserPluginFilter> Filter) const;

	/** Sorts tree items */
	void DefaultSort(const FTreeItem* InTreeItem, TArray<TSharedPtr<FTreeItem>>& InChildren);

	TArray<FName> GetDefaultPathsToExpand() const;

	/** Tell the tree that the LastExpandedPath set should be refreshed */
	void DirtyLastExpandedPaths();

	/** Update the LastExpandedPath if required */
	void UpdateLastExpandedPathsIfDirty();

	/** Create a favorites view. */
	TSharedRef<SWidget> CreateFavoritesView();

protected:
	/** A helper class to manage PreventTreeItemChangedDelegateCount by incrementing it when constructed (on the stack) and decrementing when destroyed */
	class FScopedPreventTreeItemChangedDelegate
	{
	public:
		FScopedPreventTreeItemChangedDelegate(const TSharedRef<SPathView>& InPathView)
			: PathView(InPathView)
		{
			PathView->PreventTreeItemChangedDelegateCount++;
		}

		~FScopedPreventTreeItemChangedDelegate()
		{
			check(PathView->PreventTreeItemChangedDelegateCount > 0);
			PathView->PreventTreeItemChangedDelegateCount--;
		}

	private:
		TSharedRef<SPathView> PathView;
	};

	/** A helper class to scope a selection change notification so that it only emits if the selection has actually changed after the scope ends */
	class FScopedSelectionChangedEvent
	{
	public:
		FScopedSelectionChangedEvent(const TSharedRef<SPathView>& InPathView, const bool InShouldEmitEvent = true);
		~FScopedSelectionChangedEvent();

	private:
		TSet<FName> GetSelectionSet() const;

		TSharedRef<SPathView> PathView;
		TSet<FName> InitialSelectionSet;
		bool bShouldEmitEvent = true;
	};

	/** The tree view widget */
	TSharedPtr< STreeView< TSharedPtr<FTreeItem>> > TreeViewPtr;

	/** The path view search interface */
	TSharedPtr<FSourcesSearch> SearchPtr;

	/** The list of folders in the tree */
	TArray< TSharedPtr<FTreeItem> > TreeRootItems;

	/** The The TextFilter attached to the SearchBox widget */
	TSharedPtr< FolderTextFilter > SearchBoxFolderFilter;

	/** The paths that were last reported by OnPathSelected event. Used in preserving selection when filtering folders */
	TSet<FName> LastSelectedPaths;

	/** If not empty, this is the path of the folders to sync once they are available while assets are still being discovered */
	TArray<FName> PendingInitialPaths;

	/** Delay clear until first pending path is found */
	bool bPendingInitialPathsNeedsSelectionClear = false;

	/** Context information for the folder item that is currently being created, if any */
	FContentBrowserItemTemporaryContext PendingNewFolderContext;

	TSharedPtr<SWidget> PathViewWidget;

	/** Permission filter to hide folders */
	TSharedPtr<FPathPermissionList> FolderPermissionList;

	/** Writable folder filter */
	TSharedPtr<FPathPermissionList> WritableFolderPermissionList;

	TMap<FName, TWeakPtr<FTreeItem>> TreeItemLookup;

	/** Custom Folder permissions */
	TSharedPtr<FPathPermissionList> CustomFolderPermissionList;

private:
	/** Used to track if the list of last expanded path should be updated */
	bool bLastExpandedPathsDirty = false;

	/** The paths that were last reported by OnPathExpanded event. Used in preserving expansion when filtering folders */
	TSet<FName> LastExpandedPaths;

	/** Delegate to invoke when selection changes. */
	FOnContentBrowserItemSelectionChanged OnItemSelectionChanged;

	/** Delegate to invoke when generating the context menu for an item */
	FOnGetContentBrowserItemContextMenu OnGetItemContextMenu;

	/** If > 0, the selection or expansion changed delegate will not be called. Used to update the tree from an external source or in certain bulk operations. */
	int32 PreventTreeItemChangedDelegateCount;

	/** Initial set of item categories that this view should show - may be adjusted further by things like AllowClassesFolder */
	EContentBrowserItemCategoryFilter InitialCategoryFilter;

	/** If false, the context menu will not open when right clicking an item in the tree */
	bool bAllowContextMenu;

	/** If false, the classes folder will not be added to the tree automatically */
	bool bAllowClassesFolder;

	/** If true, read only folders will be displayed */
	bool bAllowReadOnlyFolders;

	/** The title of this path view */
	FText TreeTitle;

	/** The filter collection used to filter plugins */
	TSharedPtr<FPluginFilterCollectionType> PluginPathFilters;

	/** Plugins filters that are currently active */
	TArray< TSharedRef<FContentBrowserPluginFilter> > AllPluginPathFilters;

	/** Delegate to sort with */
	FSortTreeItemChildrenDelegate SortOverride;

	/** The favorites path view if one is set. */
	TSharedPtr<SExpandableArea> FavoritesArea;

	/** The config instance to use. */
	FName OwningContentBrowserName;
};



/**
* The tree view of folders which contain favorited folders.
*/
class SFavoritePathView : public SPathView
{
public:

	virtual ~SFavoritePathView();

	/** Constructs this widget with InArgs */
	virtual void Construct(const FArguments& InArgs) override;

	virtual void Populate(const bool bIsRefreshingFilter = false) override;

	/** Saves any settings to config that should be persistent between editor sessions */
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;

	/** Loads any settings to config that should be persistent between editor sessions */
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;

	/** Adds nodes to the tree in order to construct the specified item. If bUserNamed is true, the user will name the folder and the item includes the default name. */
	virtual TSharedPtr<FTreeItem> AddFolderItem(FContentBrowserItemData&& InItem, const bool bUserNamed = false, TArray<TSharedPtr<FTreeItem>>* OutItemsCreated=nullptr) override;

	/** Updates favorites based on an external change. */
	void FixupFavoritesFromExternalChange(TArrayView<const AssetViewUtils::FMovedContentFolder> MovedFolders);

private:
	virtual TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable) override;

	/** Handles updating the view when content items are changed */
	virtual void HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems) override;

	/** Query to see whether the given path is currently filtered from the view */
	virtual bool PathIsFilteredFromViewBySearch(const FString& InPath) const override;

private:
	TArray<FString> RemovedByFolderMove;
	FDelegateHandle OnFavoritesChangedHandle;
};
