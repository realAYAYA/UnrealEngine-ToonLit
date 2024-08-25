// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPathView.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetToolsModule.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/StringView.h"
#include "ContentBrowserConfig.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserLog.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserPluginFilters.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "CoreGlobals.h"
#include "CoreTypes.h"
#include "DragDropHandler.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "HAL/PlatformTime.h"
#include "HistoryManager.h"
#include "IAssetTools.h"
#include "IContentBrowserDataModule.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"
#include "Layout/WidgetPath.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/CString.h"
#include "Misc/ComparisonUtility.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FilterCollection.h"
#include "Misc/NamePermissionList.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "PathViewTypes.h"
#include "Settings/ContentBrowserSettings.h"
#include "SlotBase.h"
#include "SourcesData.h"
#include "SourcesSearch.h"
#include "SourcesViewWidgets.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "Trace/Detail/Channel.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STableRow.h"

class FDragDropOperation;
class SWidget;
struct FAssetData;
struct FGeometry;

#define LOCTEXT_NAMESPACE "ContentBrowser"

SPathView::FScopedSelectionChangedEvent::FScopedSelectionChangedEvent(const TSharedRef<SPathView>& InPathView, const bool InShouldEmitEvent)
	: PathView(InPathView)
	, bShouldEmitEvent(InShouldEmitEvent)
{
	PathView->PreventTreeItemChangedDelegateCount++;
	InitialSelectionSet = GetSelectionSet();
}

SPathView::FScopedSelectionChangedEvent::~FScopedSelectionChangedEvent()
{
	check(PathView->PreventTreeItemChangedDelegateCount > 0);
	PathView->PreventTreeItemChangedDelegateCount--;

	if (bShouldEmitEvent)
	{
		const TSet<FName> FinalSelectionSet = GetSelectionSet();
		const bool bHasSelectionChanges = InitialSelectionSet.Num() != FinalSelectionSet.Num() || InitialSelectionSet.Difference(FinalSelectionSet).Num() > 0;
		if (bHasSelectionChanges)
		{
			const TArray<TSharedPtr<FTreeItem>> SelectedItems = PathView->TreeViewPtr->GetSelectedItems();
			PathView->TreeSelectionChanged(SelectedItems.Num() > 0 ? SelectedItems[0] : nullptr, ESelectInfo::Direct);
		}
	}
}

TSet<FName> SPathView::FScopedSelectionChangedEvent::GetSelectionSet() const
{
	TSet<FName> SelectionSet;

	const TArray<TSharedPtr<FTreeItem>> SelectedItems = PathView->TreeViewPtr->GetSelectedItems();
	for (const TSharedPtr<FTreeItem>& Item : SelectedItems)
	{
		if (ensure(Item.IsValid()))
		{
			SelectionSet.Add(Item->GetItem().GetVirtualPath());
		}
	}

	return SelectionSet;
}

SPathView::~SPathView()
{
	if (IContentBrowserDataModule* ContentBrowserDataModule = IContentBrowserDataModule::GetPtr())
	{
		if (UContentBrowserDataSubsystem* ContentBrowserData = ContentBrowserDataModule->GetSubsystem())
		{
			ContentBrowserData->OnItemDataUpdated().RemoveAll(this);
			ContentBrowserData->OnItemDataRefreshed().RemoveAll(this);
			ContentBrowserData->OnItemDataDiscoveryComplete().RemoveAll(this);
		}
	}

	SearchBoxFolderFilter->OnChanged().RemoveAll( this );
}

void SPathView::Construct( const FArguments& InArgs )
{
	OwningContentBrowserName = InArgs._OwningContentBrowserName;
	OnItemSelectionChanged = InArgs._OnItemSelectionChanged;
	bAllowContextMenu = InArgs._AllowContextMenu;
	OnGetItemContextMenu = InArgs._OnGetItemContextMenu;
	InitialCategoryFilter = InArgs._InitialCategoryFilter;
	bAllowClassesFolder = InArgs._AllowClassesFolder;
	bAllowReadOnlyFolders = InArgs._AllowReadOnlyFolders;
	PreventTreeItemChangedDelegateCount = 0;
	TreeTitle = LOCTEXT("AssetTreeTitle", "Asset Tree");
	if ( InArgs._FocusSearchBoxWhenOpened )
	{
		RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SPathView::SetFocusPostConstruct ) );
	}

	SortOverride = FSortTreeItemChildrenDelegate::CreateSP(this, &SPathView::DefaultSort);

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	ContentBrowserData->OnItemDataUpdated().AddSP(this, &SPathView::HandleItemDataUpdated);
	ContentBrowserData->OnItemDataRefreshed().AddSP(this, &SPathView::HandleItemDataRefreshed);
	ContentBrowserData->OnItemDataDiscoveryComplete().AddSP(this, &SPathView::HandleItemDataDiscoveryComplete);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FolderPermissionList = AssetToolsModule.Get().GetFolderPermissionList();
	WritableFolderPermissionList = AssetToolsModule.Get().GetWritableFolderPermissionList();

	// Listen for when view settings are changed
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetOnContentBrowserSettingChanged().AddSP(this, &SPathView::HandleSettingChanged);

	//Setup the SearchBox filter
	SearchBoxFolderFilter = MakeShareable( new FolderTextFilter( FolderTextFilter::FItemToStringArray::CreateSP( this, &SPathView::PopulateFolderSearchStrings ) ) );
	SearchBoxFolderFilter->OnChanged().AddSP( this, &SPathView::FilterUpdated );

	// Setup plugin filters
	PluginPathFilters = InArgs._PluginPathFilters;
	if (PluginPathFilters.IsValid())
	{
		// Add all built-in filters here
		AllPluginPathFilters.Add( MakeShareable(new FContentBrowserPluginFilter_ContentOnlyPlugins()) );

		// Add external filters
		for (const FContentBrowserModule::FAddPathViewPluginFilters& Delegate : ContentBrowserModule.GetAddPathViewPluginFilters())
		{
			if (Delegate.IsBound())
			{
				Delegate.Execute(AllPluginPathFilters);
			}
		}
	}

	if (!TreeViewPtr.IsValid())
	{
		SAssignNew(TreeViewPtr, STreeView< TSharedPtr<FTreeItem> >)
			.TreeItemsSource(&TreeRootItems)
			.OnGenerateRow(this, &SPathView::GenerateTreeRow)
			.OnItemScrolledIntoView(this, &SPathView::TreeItemScrolledIntoView)
			.ItemHeight(18)
			.SelectionMode(InArgs._SelectionMode)
			.OnSelectionChanged(this, &SPathView::TreeSelectionChanged)
			.OnExpansionChanged(this, &SPathView::TreeExpansionChanged)
			.OnGetChildren(this, &SPathView::GetChildrenForTree)
			.OnSetExpansionRecursive(this, &SPathView::SetTreeItemExpansionRecursive)
			.OnContextMenuOpening(this, &SPathView::MakePathViewContextMenu)
			.ClearSelectionOnClick(false)
			.HighlightParentNodesForSelection(true);
	}

	SearchPtr = InArgs._ExternalSearch;
	if (!SearchPtr)
	{
		SearchPtr = MakeShared<FSourcesSearch>();
		SearchPtr->Initialize();
		SearchPtr->SetHintText(LOCTEXT("AssetTreeSearchBoxHint", "Search Folders"));
	}
	SearchPtr->OnSearchChanged().AddSP(this, &SPathView::SetSearchFilterText);

	TSharedRef<SBox> SearchBox = SNew(SBox);
	if (!InArgs._ExternalSearch)
	{
		SearchBox->SetContent(
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				InArgs._SearchContent.Widget
			]

			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.Visibility(InArgs._SearchBarVisibility)
				[
					SearchPtr->GetWidget()
				]
			]
		);
	}

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox);

	if (!InArgs._ExternalSearch || InArgs._ShowTreeTitle)
	{
		ContentBox->AddSlot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(8.f)
			[
				SNew(SVerticalBox)

				// Search
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SearchBox
				]

				// Tree title
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font( FAppStyle::GetFontStyle("ContentBrowser.SourceTitleFont") )
					.Text(this, &SPathView::GetTreeTitle)
					.Visibility(InArgs._ShowTreeTitle ? EVisibility::Visible : EVisibility::Collapsed)
				]
			]
		];
	}

	// Separator
	if (InArgs._ShowSeparator)
	{
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 1)
		[
			SNew(SSeparator)
		];
	}

	if (InArgs._ShowFavorites)
	{
		ContentBox->AddSlot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.SizeRule_Lambda([this]()
				{ 
					return (FavoritesArea.IsValid() && FavoritesArea->IsExpanded()) ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent;
				})
			.MinSize(24)
			.Value(0.25f)
			[
				CreateFavoritesView()
			]
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				TreeViewPtr.ToSharedRef()
			]
		];
	}
	else
	{
		// Tree
		ContentBox->AddSlot()
		.FillHeight(1.f)
		[
			TreeViewPtr.ToSharedRef()
		];
	}

	ChildSlot
	[
		ContentBox
	];

	CustomFolderPermissionList = InArgs._CustomFolderPermissionList;
	// Add all paths currently gathered from the asset registry
	Populate();

	for (const FName PathToExpand : GetDefaultPathsToExpand())
	{
		if (TSharedPtr<FTreeItem> FoundItem = FindTreeItem(PathToExpand))
		{
			RecursiveExpandParents(FoundItem);
			TreeViewPtr->SetItemExpansion(FoundItem, true);
		}
	}
}

void SPathView::PopulatePathViewFiltersMenu(UToolMenu* Menu)
{
	{
		FToolMenuSection& Section = Menu->AddSection("Reset");
		Section.AddMenuEntry(
			"ResetPluginPathFilters",
			LOCTEXT("ResetPluginPathFilters_Label", "Reset Path View Filters"),
			LOCTEXT("ResetPluginPathFilters_Tooltip", "Reset current path view filters state"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SPathView::ResetPluginPathFilters))
		);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Filters", LOCTEXT("PathViewFilters_Label", "Filters"));

		for (const TSharedRef<FContentBrowserPluginFilter>& Filter : AllPluginPathFilters)
		{
			Section.AddMenuEntry(
				NAME_None,
				Filter->GetDisplayName(),
				Filter->GetToolTipText(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), Filter->GetIconName()),
				FUIAction(
					FExecuteAction::CreateSP(this, &SPathView::PluginPathFilterClicked, Filter),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SPathView::IsPluginPathFilterInUse, Filter)
				),
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

void SPathView::PluginPathFilterClicked(TSharedRef<FContentBrowserPluginFilter> Filter)
{
	SetPluginPathFilterActive(Filter, !IsPluginPathFilterInUse(Filter));
	Populate();
}

bool SPathView::IsPluginPathFilterInUse(TSharedRef<FContentBrowserPluginFilter> Filter) const
{
	for (int32 i=0; i < PluginPathFilters->Num(); ++i)
	{
		if (PluginPathFilters->GetFilterAtIndex(i) == Filter)
		{
			return true;
		}
	}

	return false;
}

void SPathView::ResetPluginPathFilters()
{
	for (const TSharedRef<FContentBrowserPluginFilter>& Filter : AllPluginPathFilters)
	{
		SetPluginPathFilterActive(Filter, false);
	}

	Populate();
}

void SPathView::SetPluginPathFilterActive(const TSharedRef<FContentBrowserPluginFilter>& Filter, bool bActive)
{
	if (Filter->IsInverseFilter())
	{
		//Inverse filters are active when they are "disabled"
		bActive = !bActive;
	}

	Filter->ActiveStateChanged(bActive);

	if (bActive)
	{
		PluginPathFilters->Add(Filter);
	}
	else
	{
		PluginPathFilters->Remove(Filter);
	}

	if (FPathViewConfig* PathViewConfig = GetPathViewConfig())
	{
		if (bActive)
		{
			PathViewConfig->PluginFilters.Add(Filter->GetName());
		}
		else
		{
			PathViewConfig->PluginFilters.Remove(Filter->GetName());
		}
		
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}
}

FPathViewConfig* SPathView::GetPathViewConfig() const
{
	if (OwningContentBrowserName.IsNone())
	{
		return nullptr;
	}

	FContentBrowserInstanceConfig* Config = UContentBrowserConfig::Get()->Instances.Find(OwningContentBrowserName);
	if (Config == nullptr)
	{
		return nullptr;
	}
	
	return &Config->PathView;
}

FContentBrowserInstanceConfig* SPathView::GetContentBrowserConfig() const
{
	if (OwningContentBrowserName.IsNone())
	{
		return nullptr;
	}

	FContentBrowserInstanceConfig* Config = UContentBrowserConfig::Get()->Instances.Find(OwningContentBrowserName);
	if (Config == nullptr)
	{
		return nullptr;
	}

	return Config;
}

void SPathView::SetSelectedPaths(const TArray<FName>& Paths)
{
	TArray<FString> PathStrings;
	Algo::Transform(Paths, PathStrings, [](const FName& Name) { return Name.ToString(); });
	SetSelectedPaths(PathStrings);
}

void SPathView::SetSelectedPaths(const TArray<FString>& Paths)
{
	if (!ensure(TreeViewPtr.IsValid()))
	{
		return;
	}

	// Clear the search box if it potentially hides a path we want to select
	for (const FString& Path : Paths)
	{
		if (PathIsFilteredFromViewBySearch(Path))
		{
			SearchPtr->ClearSearch();
			break;
		}
	}

	// Prevent the selection changed delegate since the invoking code requested it
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// If the selection was changed before all pending initial paths were found, stop attempting to select them
	PendingInitialPaths.Empty();
	bPendingInitialPathsNeedsSelectionClear = false;

	// Clear the selection to start, then add the selected paths as they are found
	LastSelectedPaths.Empty();
	TreeViewPtr->ClearSelection();

	for (const FString& Path : Paths)
	{
		TArray<FName> PathItemList;
		{
			TArray<FString> PathItemListStr;
			Path.ParseIntoArray(PathItemListStr, TEXT("/"), /*InCullEmpty=*/true);

			PathItemList.Reserve(PathItemListStr.Num());
			for (const FString& PathItemName : PathItemListStr)
			{
				PathItemList.Add(*PathItemName);
			}
		}

		if (PathItemList.Num())
		{
			// There is at least one element in the path
			TArray<TSharedPtr<FTreeItem>> TreeItems;

			// Find the first item in the root items list
			for (const TSharedPtr<FTreeItem>& TreeItem : TreeRootItems)
			{
				if (TreeItem->GetItem().GetItemName() == PathItemList[0])
				{
					// Found the first item in the path
					TreeItems.Add(TreeItem);
					break;
				}
			}

			// If found in the root items list, try to find the childmost item matching the path
			if (TreeItems.Num() > 0)
			{
				for ( int32 PathItemIdx = 1; PathItemIdx < PathItemList.Num(); ++PathItemIdx )
				{
					const FName PathItemName = PathItemList[PathItemIdx];
					const TSharedPtr<FTreeItem> ChildItem = TreeItems.Last()->GetChild(PathItemName);
					if (ChildItem.IsValid())
					{
						// Update tree items list
						TreeItems.Add(ChildItem);
					}
					else
					{
						// Could not find the child item
						break;
					}
				}

				// Expand all the tree folders up to but not including the last one.
				for (int32 ItemIdx = 0; ItemIdx < TreeItems.Num() - 1; ++ItemIdx)
				{
					TreeViewPtr->SetItemExpansion(TreeItems[ItemIdx], true);
				}

				// Set the selection to the closest found folder and scroll it into view
				LastSelectedPaths.Add(TreeItems.Last()->GetItem().GetInvariantPath());
				TreeViewPtr->SetItemSelection(TreeItems.Last(), true);
				TreeViewPtr->RequestScrollIntoView(TreeItems.Last());
			}
			else
			{
				// Could not even find the root path... skip
			}
		}
		else
		{
			// No path items... skip
		}
	}

	if (FPathViewConfig* PathViewConfig = GetPathViewConfig())
	{
		PathViewConfig->SelectedPaths = LastSelectedPaths.Array();

		UContentBrowserConfig::Get()->SaveEditorConfig();
	}
}

void SPathView::ClearSelection()
{
	// Prevent the selection changed delegate since the invoking code requested it
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// If the selection was changed before all pending initial paths were found, stop attempting to select them
	PendingInitialPaths.Empty();
	bPendingInitialPathsNeedsSelectionClear = false;

	// Clear the selection to start, then add the selected paths as they are found
	TreeViewPtr->ClearSelection();
}

FString SPathView::GetSelectedPath() const
{
	// TODO: Abstract away?
	TArray<TSharedPtr<FTreeItem>> Items = TreeViewPtr->GetSelectedItems();
	if ( Items.Num() > 0 )
	{
		return Items[0]->GetItem().GetVirtualPath().ToString();
	}

	return FString();
}

TArray<FString> SPathView::GetSelectedPaths() const
{
	TArray<FString> RetArray;

	// TODO: Abstract away?
	TArray<TSharedPtr<FTreeItem>> Items = TreeViewPtr->GetSelectedItems();
	for ( int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx )
	{
		RetArray.Add(Items[ItemIdx]->GetItem().GetVirtualPath().ToString());
	}

	return RetArray;
}

TArray<FContentBrowserItem> SPathView::GetSelectedFolderItems() const
{
	TArray<TSharedPtr<FTreeItem>> SelectedViewItems = TreeViewPtr->GetSelectedItems();

	TArray<FContentBrowserItem> SelectedFolders;
	for (const TSharedPtr<FTreeItem>& SelectedViewItem : SelectedViewItems)
	{
		if (!SelectedViewItem->GetItem().IsTemporary())
		{
			SelectedFolders.Emplace(SelectedViewItem->GetItem());
		}
	}
	return SelectedFolders;
}

TSharedPtr<FTreeItem> SPathView::AddFolderItem(FContentBrowserItemData&& InItem, const bool bUserNamed, TArray<TSharedPtr<FTreeItem>>* OutItemsCreated)
{
	if (!ensure(TreeViewPtr.IsValid()))
	{
		// No tree view for some reason
		return nullptr;
	}

	if (!InItem.IsFolder())
	{
		// Not a folder
		return nullptr;
	}

	// Clear selection if user has not changed it yet or this is the first pending initial path being selected
	auto SelectingPendingInitialPath = [this]()
	{
		if (bPendingInitialPathsNeedsSelectionClear)
		{
			bPendingInitialPathsNeedsSelectionClear = false;

			LastSelectedPaths.Empty();
			TreeViewPtr->ClearSelection();
		}
	};

	// The path view will add a node for each level of the path tree
	TArray<FString> PathItemList;
	InItem.GetVirtualPath().ToString().ParseIntoArray(PathItemList, TEXT("/"), /*InCullEmpty=*/true);

	// Start at the root and work down until all required children have been added
	TSharedPtr<FTreeItem> ParentTreeItem;
	TArray<TSharedPtr<FTreeItem>>* CurrentTreeItems = &TreeRootItems;

	TStringBuilder<512> CurrentPathStr;
	CurrentPathStr.Append(TEXT("/"));
	for (int32 PathItemIndex = 0; PathItemIndex < PathItemList.Num(); ++PathItemIndex)
	{
		const bool bIsLeafmostItem = PathItemIndex == PathItemList.Num() - 1;

		const FString& FolderNameStr = PathItemList[PathItemIndex];
		const FName FolderName = *FolderNameStr;
		FPathViews::Append(CurrentPathStr, FolderNameStr);

		// Try and find an existing tree item
		TSharedPtr<FTreeItem> CurrentTreeItem;
		for (const TSharedPtr<FTreeItem>& PotentialTreeItem : *CurrentTreeItems)
		{
			if (PotentialTreeItem->GetItem().GetItemName() == FolderName)
			{
				CurrentTreeItem = PotentialTreeItem;
				break;
			}
		}

		// Handle creating the leaf-most item that was given to us to create
		if (bIsLeafmostItem)
		{
			if (CurrentTreeItem)
			{
				// Found a match - merge the new item data
				CurrentTreeItem->AppendItemData(InItem);
			}
			else
			{
				// No match - create a new item
				CurrentTreeItem = MakeShared<FTreeItem>(MoveTemp(InItem));
				CurrentTreeItem->Parent = ParentTreeItem;
				CurrentTreeItem->SetSortOverride(SortOverride);
				CurrentTreeItems->Add(CurrentTreeItem);
				if (OutItemsCreated)
				{
					OutItemsCreated->Add(CurrentTreeItem);
				}

				TreeItemLookup.Add(CurrentTreeItem->GetItem().GetVirtualPath(), CurrentTreeItem);

				if (ParentTreeItem)
				{
					check(&ParentTreeItem->Children == CurrentTreeItems);
					ParentTreeItem->RequestSortChildren();
				}
				else
				{
					SortRootItems();
				}

				// If we have pending initial paths, and this path added the path, we should select it now
				if (PendingInitialPaths.Num() > 0 && PendingInitialPaths.Contains(CurrentTreeItem->GetItem().GetVirtualPath()))
				{
					SelectingPendingInitialPath();
					RecursiveExpandParents(CurrentTreeItem);
					TreeViewPtr->SetItemSelection(CurrentTreeItem, true);
					TreeViewPtr->RequestScrollIntoView(CurrentTreeItem);
				}
			}

			// If we want to name this item, select it, scroll it into view, expand the parent
			if (bUserNamed)
			{
				RecursiveExpandParents(CurrentTreeItem);
				TreeViewPtr->SetSelection(CurrentTreeItem);
				CurrentTreeItem->SetNamingFolder(true);
				TreeViewPtr->RequestScrollIntoView(CurrentTreeItem);
			}

			TreeViewPtr->RequestTreeRefresh();
			return CurrentTreeItem;
		}

		// If we're missing an item on the way down to the leaf-most item then we'll add a placeholder
		// This shouldn't usually happen as Populate will create paths in the correct order, but 
		// the path picker may force add a path that hasn't been discovered (or doesn't exist) yet
		if (!CurrentTreeItem)
		{
			CurrentTreeItem = MakeShared<FTreeItem>(FContentBrowserItemData(InItem.GetOwnerDataSource(), EContentBrowserItemFlags::Type_Folder, *CurrentPathStr, FolderName, FText(), nullptr));
			CurrentTreeItem->Parent = ParentTreeItem;
			CurrentTreeItem->SetSortOverride(SortOverride);
			CurrentTreeItems->Add(CurrentTreeItem);
			if (OutItemsCreated)
			{
				OutItemsCreated->Add(CurrentTreeItem);
			}

			TreeItemLookup.Add(CurrentTreeItem->GetItem().GetVirtualPath(), CurrentTreeItem);

			if (ParentTreeItem)
			{
				check(&ParentTreeItem->Children == CurrentTreeItems);
				ParentTreeItem->RequestSortChildren();
			}
			else
			{
				SortRootItems();
			}

			// If we have pending initial paths, and this path added the path, we should select it now
			if (PendingInitialPaths.Num() > 0 && PendingInitialPaths.Contains(CurrentTreeItem->GetItem().GetVirtualPath()))
			{
				SelectingPendingInitialPath();
				RecursiveExpandParents(CurrentTreeItem);
				TreeViewPtr->SetItemSelection(CurrentTreeItem, true);
				TreeViewPtr->RequestScrollIntoView(CurrentTreeItem);
			}
		}

		// Set-up the data for the next level
		ParentTreeItem = CurrentTreeItem;
		CurrentTreeItems = &ParentTreeItem->Children;
	}

	return nullptr;
}

bool SPathView::RemoveFolderItem(const FContentBrowserItemData& InItem)
{
	if (!ensure(TreeViewPtr.IsValid()))
	{
		// No tree view for some reason
		return false;
	}

	if (!InItem.IsFolder())
	{
		// Not a folder
		return false;
	}

	// Find the folder in the tree
	const FName VirtualPath = InItem.GetVirtualPath();
	if (TSharedPtr<FTreeItem> ItemToRemove = FindTreeItem(VirtualPath))
	{
		// Only fully remove this item if every sub-item is removed (items become invalid when empty)
		ItemToRemove->RemoveItemData(InItem);
		if (ItemToRemove->GetItem().IsValid())
		{
			return true;
		}

		// Found the folder to remove. Remove it.
		if (TSharedPtr<FTreeItem> ItemParent = ItemToRemove->Parent.Pin())
		{
			// Remove the folder from its parent's list
			ItemParent->Children.Remove(ItemToRemove);
		}
		else
		{
			// This is a root item. Remove the folder from the root items list.
			TreeRootItems.Remove(ItemToRemove);
		}

		TreeItemLookup.Remove(VirtualPath);

		// Refresh the tree
		TreeViewPtr->RequestTreeRefresh();

		return true;
	}
	
	// Did not find the folder to remove
	return false;
}

void SPathView::RenameFolderItem(const FContentBrowserItem& InItem)
{
	if (!ensure(TreeViewPtr.IsValid()))
	{
		// No tree view for some reason
		return;
	}

	if (!InItem.IsFolder())
	{
		// Not a folder
		return;
	}

	// Find the folder in the tree
	if (TSharedPtr<FTreeItem> ItemToRename = FindTreeItem(InItem.GetVirtualPath()))
	{
		ItemToRename->SetNamingFolder(true);

		TreeViewPtr->SetSelection(ItemToRename);
		TreeViewPtr->RequestScrollIntoView(ItemToRename);
	}
}

FContentBrowserDataCompiledFilter SPathView::CreateCompiledFolderFilter() const
{
	const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
	bool bDisplayPluginFolders = ContentBrowserSettings->GetDisplayPluginFolders();
	// check to see if we have an instance config that overrides the default in UContentBrowserSettings
	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bDisplayPluginFolders = EditorConfig->bShowPluginContent;
	}

	FContentBrowserDataFilter DataFilter;
	DataFilter.bRecursivePaths = true;
	DataFilter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFolders;
	DataFilter.ItemCategoryFilter = GetContentBrowserItemCategoryFilter();
	DataFilter.ItemAttributeFilter = GetContentBrowserItemAttributeFilter();

	TSharedPtr<FPathPermissionList> CombinedFolderPermissionList = ContentBrowserUtils::GetCombinedFolderPermissionList(FolderPermissionList, bAllowReadOnlyFolders ? nullptr : WritableFolderPermissionList);

	if (CustomFolderPermissionList.IsValid())
	{
		if (!CombinedFolderPermissionList.IsValid())
		{
			CombinedFolderPermissionList = MakeShared<FPathPermissionList>();
		}
		CombinedFolderPermissionList->Append(*CustomFolderPermissionList);
	}

	if (PluginPathFilters.IsValid() && PluginPathFilters->Num() > 0 && bDisplayPluginFolders)
	{
		TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPluginsWithContent();
		for (const TSharedRef<IPlugin>& Plugin : Plugins)
		{
			if (!PluginPathFilters->PassesAllFilters(Plugin))
			{
				FString MountedAssetPath = Plugin->GetMountedAssetPath();
				MountedAssetPath.RemoveFromEnd(TEXT("/"), ESearchCase::CaseSensitive);

				if (!CombinedFolderPermissionList.IsValid())
				{
					CombinedFolderPermissionList = MakeShared<FPathPermissionList>();
				}
				CombinedFolderPermissionList->AddDenyListItem("PluginPathFilters", MountedAssetPath);
			}
		}
	}

	ContentBrowserUtils::AppendAssetFilterToContentBrowserFilter(FARFilter(), nullptr, CombinedFolderPermissionList, DataFilter);

	FContentBrowserDataCompiledFilter CompiledDataFilter;
	{
		static const FName RootPath = "/";
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		ContentBrowserData->CompileFilter(RootPath, DataFilter, CompiledDataFilter);
	}
	return CompiledDataFilter;
}

EContentBrowserItemCategoryFilter SPathView::GetContentBrowserItemCategoryFilter() const
{
	const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
	bool bDisplayCppFolders = ContentBrowserSettings->GetDisplayCppFolders();
	// check to see if we have an instance config that overrides the default in UContentBrowserSettings
	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bDisplayCppFolders = EditorConfig->bShowCppFolders;
	}

	EContentBrowserItemCategoryFilter ItemCategoryFilter = InitialCategoryFilter;
	if (bAllowClassesFolder && bDisplayCppFolders)
	{
		ItemCategoryFilter |= EContentBrowserItemCategoryFilter::IncludeClasses;
	}
	else
	{
		ItemCategoryFilter &= ~EContentBrowserItemCategoryFilter::IncludeClasses;
	}
	ItemCategoryFilter &= ~EContentBrowserItemCategoryFilter::IncludeCollections;

	return ItemCategoryFilter;
}

EContentBrowserItemAttributeFilter SPathView::GetContentBrowserItemAttributeFilter() const
{
	const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
	bool bDisplayEngineContent = ContentBrowserSettings->GetDisplayEngineFolder();
	bool bDisplayPluginContent = ContentBrowserSettings->GetDisplayPluginFolders();
	bool bDisplayDevelopersContent = ContentBrowserSettings->GetDisplayDevelopersFolder();
	bool bDisplayL10NContent = ContentBrowserSettings->GetDisplayL10NFolder();
	
	// check to see if we have an instance config that overrides the defaults in UContentBrowserSettings
	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bDisplayEngineContent = EditorConfig->bShowEngineContent;
		bDisplayPluginContent = EditorConfig->bShowPluginContent;
		bDisplayDevelopersContent = EditorConfig->bShowDeveloperContent;
		bDisplayL10NContent = EditorConfig->bShowLocalizedContent;
	}
	
	return EContentBrowserItemAttributeFilter::IncludeProject
			| (bDisplayEngineContent ? EContentBrowserItemAttributeFilter::IncludeEngine : EContentBrowserItemAttributeFilter::IncludeNone)
			| (bDisplayPluginContent ? EContentBrowserItemAttributeFilter::IncludePlugins : EContentBrowserItemAttributeFilter::IncludeNone)
			| (bDisplayDevelopersContent ? EContentBrowserItemAttributeFilter::IncludeDeveloper : EContentBrowserItemAttributeFilter::IncludeNone)
			| (bDisplayL10NContent ? EContentBrowserItemAttributeFilter::IncludeLocalized : EContentBrowserItemAttributeFilter::IncludeNone);
}

bool SPathView::InternalPathPassesBlockLists(const FStringView InInternalPath, const int32 InAlreadyCheckedDepth) const
{
	TArray<const FPathPermissionList*, TInlineAllocator<2>> BlockLists;
	if (FolderPermissionList.IsValid() && FolderPermissionList->HasFiltering())
	{
		BlockLists.Add(FolderPermissionList.Get());
	}

	if (!bAllowReadOnlyFolders && WritableFolderPermissionList.IsValid() && WritableFolderPermissionList->HasFiltering())
	{
		BlockLists.Add(WritableFolderPermissionList.Get());
	}

	for (const FPathPermissionList* Filter : BlockLists)
	{
		if (!Filter->PassesStartsWithFilter(InInternalPath))
		{
			return false;
		}
	}

	if (InAlreadyCheckedDepth < 1 && PluginPathFilters.IsValid() && PluginPathFilters->Num() > 0)
	{
		const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
		bool bDisplayPluginFolders = ContentBrowserSettings->GetDisplayPluginFolders();

		// check to see if we have an instance config that overrides the default in UContentBrowserSettings
		if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
		{
			bDisplayPluginFolders = EditorConfig->bShowPluginContent;
		}

		if (bDisplayPluginFolders)
		{
			const FStringView FirstFolderName = FPathViews::GetMountPointNameFromPath(InInternalPath);
			if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(FirstFolderName))
			{
				if (!PluginPathFilters->PassesAllFilters(Plugin.ToSharedRef()))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void SPathView::SyncToItems(TArrayView<const FContentBrowserItem> ItemsToSync, const bool bAllowImplicitSync)
{
	TArray<FName> VirtualPathsToSync;
	for (const FContentBrowserItem& Item : ItemsToSync)
	{
		if (Item.IsFile())
		{
			// Files need to sync their parent folder in the tree, so chop off the end of their path
			VirtualPathsToSync.Add(*FPaths::GetPath(Item.GetVirtualPath().ToString()));
		}
		else
		{
			VirtualPathsToSync.Add(Item.GetVirtualPath());
		}
	}

	SyncToVirtualPaths(VirtualPathsToSync, bAllowImplicitSync);
}

void SPathView::SyncToVirtualPaths(TArrayView<const FName> VirtualPathsToSync, const bool bAllowImplicitSync)
{
	// Clear the search box if it potentially hides a path we want to select
	for (const FName& VirtualPathToSync : VirtualPathsToSync)
	{
		if (PathIsFilteredFromViewBySearch(VirtualPathToSync.ToString()))
		{
			SearchPtr->ClearSearch();
			break;
		}
	}

	TArray<TSharedPtr<FTreeItem>> SyncTreeItems;
	{
		TSet<FName> UniqueVirtualPathsToSync;
		for (const FName& VirtualPathToSync : VirtualPathsToSync)
		{
			if (!UniqueVirtualPathsToSync.Contains(VirtualPathToSync))
			{
				UniqueVirtualPathsToSync.Add(VirtualPathToSync);

				TSharedPtr<FTreeItem> Item = FindTreeItem(VirtualPathToSync);
				if (Item.IsValid())
				{
					SyncTreeItems.Add(Item);
				}
			}
		}
	}

	if ( SyncTreeItems.Num() > 0 )
	{
		// Batch the selection changed event
		FScopedSelectionChangedEvent ScopedSelectionChangedEvent(SharedThis(this));

		if (bAllowImplicitSync)
		{
			// Prune the current selection so that we don't unnecessarily change the path which might disorientate the user.
			// If a parent tree item is currently selected we don't need to clear it and select the child
			TArray<TSharedPtr<FTreeItem>> SelectedTreeItems = TreeViewPtr->GetSelectedItems();

			for (int32 Index = 0; Index < SelectedTreeItems.Num(); ++Index)
			{
				// For each item already selected in the tree
				const TSharedPtr<FTreeItem>& AlreadySelectedTreeItem = SelectedTreeItems[Index];
				if (!AlreadySelectedTreeItem.IsValid())
				{
					continue;
				}

				// Check to see if any of the items to sync are already synced
				for (int32 ToSyncIndex = SyncTreeItems.Num()-1; ToSyncIndex >= 0; --ToSyncIndex)
				{
					const TSharedPtr<FTreeItem>& ToSyncItem = SyncTreeItems[ToSyncIndex];
					if (ToSyncItem == AlreadySelectedTreeItem || ToSyncItem->IsChildOf(*AlreadySelectedTreeItem.Get()))
					{
						// A parent is already selected
						SyncTreeItems.Pop();
					}
					else if (ToSyncIndex == 0)
					{
						// AlreadySelectedTreeItem is not required for SyncTreeItems, so deselect it
						TreeViewPtr->SetItemSelection(AlreadySelectedTreeItem, false);
					}
				}
			}
		}
		else
		{
			// Explicit sync so just clear the selection
			TreeViewPtr->ClearSelection();
		}

		// SyncTreeItems should now only contain items which aren't already shown explicitly or implicitly (as a child)
		for (const TSharedPtr<FTreeItem>& Item : SyncTreeItems)
		{
			RecursiveExpandParents(Item);
			TreeViewPtr->SetItemSelection(Item, true);
		}
	}

	// > 0 as some may have been popped off in the code above
	if (SyncTreeItems.Num() > 0)
	{
		// Scroll the first item into view if applicable
		TreeViewPtr->RequestScrollIntoView(SyncTreeItems[0]);
	}
}

void SPathView::SyncToLegacy(TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bAllowImplicitSync)
{
	TArray<FName> VirtualPathsToSync;
	ContentBrowserUtils::ConvertLegacySelectionToVirtualPaths(AssetDataList, FolderList, /*UseFolderPaths*/true, VirtualPathsToSync);

	SyncToVirtualPaths(VirtualPathsToSync, bAllowImplicitSync);
}

void SPathView::ClearTreeItems()
{
	TreeRootItems.Empty();
	TreeViewPtr->ClearSelection();
	TreeItemLookup.Empty();
}

TSharedPtr<FTreeItem> SPathView::FindTreeItem(FName InPath) const
{
	if (const TWeakPtr<FTreeItem>* FoundWeak = TreeItemLookup.Find(InPath))
	{
		return FoundWeak->Pin();
	}

	return TSharedPtr<FTreeItem>();
}

void SPathView::ApplyHistoryData( const FHistoryData& History )
{
	// Prevent the selection changed delegate because it would add more history when we are just setting a state
	FScopedPreventTreeItemChangedDelegate DelegatePrevention( SharedThis(this) );

	// Update paths
	TArray<FString> SelectedPaths;
	for (const FName& HistoryPath : History.SourcesData.VirtualPaths)
	{
		SelectedPaths.Add(HistoryPath.ToString());
	}
	SetSelectedPaths(SelectedPaths);
}

void SPathView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& InstanceName) const
{
	FString SelectedPathsString;
	TArray< TSharedPtr<FTreeItem> > PathItems = TreeViewPtr->GetSelectedItems();

	for (const TSharedPtr<FTreeItem>& Item : PathItems)
	{
		if (SelectedPathsString.Len() > 0)
		{
			SelectedPathsString += TEXT(",");
		}

		FName InvariantPath;
		IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(Item->GetItem().GetVirtualPath(), InvariantPath);
		InvariantPath.AppendString(SelectedPathsString);
	}

	GConfig->SetString(*IniSection, *(InstanceName + TEXT(".SelectedPaths")), *SelectedPathsString, IniFilename);

	FString PluginFiltersString;
	if (PluginPathFilters.IsValid())
	{
		for (int32 FilterIdx = 0; FilterIdx < PluginPathFilters->Num(); ++FilterIdx)
		{
			if (PluginFiltersString.Len() > 0)
			{
				PluginFiltersString += TEXT(",");
			}

			TSharedPtr<FContentBrowserPluginFilter> Filter = StaticCastSharedPtr<FContentBrowserPluginFilter>(PluginPathFilters->GetFilterAtIndex(FilterIdx));
			PluginFiltersString += Filter->GetName();
		}
		GConfig->SetString(*IniSection, *(InstanceName + TEXT(".PluginFilters")), *PluginFiltersString, IniFilename);
	}
}

void SPathView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	// Selected Paths
	TArray<FName> NewSelectedPaths;
	if (FPathViewConfig* PathViewConfig = GetPathViewConfig())
	{
		NewSelectedPaths = PathViewConfig->SelectedPaths;
	}
	else 
	{
		FString SelectedPathsString;
		if (GConfig->GetString(*IniSection, *(SettingsString + TEXT(".SelectedPaths")), SelectedPathsString, IniFilename))
		{
			TArray<FString> ParsedPaths;
			SelectedPathsString.ParseIntoArray(ParsedPaths, TEXT(","), /*bCullEmpty*/true);

			Algo::Transform(ParsedPaths, NewSelectedPaths, [](const FString& Str) { return *Str; });
		}
	}

	// Replace each path in NewSelectedPaths with virtual version of that path
	for (FName& Path : NewSelectedPaths)
	{
		IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(Path, Path);
	}

	{
		// Batch the selection changed event
		FScopedSelectionChangedEvent ScopedSelectionChangedEvent(SharedThis(this));

		bPendingInitialPathsNeedsSelectionClear = false;

		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		if (ContentBrowserData->IsDiscoveringItems())
		{
			// Determine if any of the items already exist
			bool bFoundAnyItemInTree = false;
			for (const FName& Path : NewSelectedPaths)
			{
				if (TSharedPtr<FTreeItem> FoundItem = FindTreeItem(Path))
				{
					bFoundAnyItemInTree = true;
					break;
				}
			}

			if (bFoundAnyItemInTree)
			{
				// Clear any previously selected paths
				LastSelectedPaths.Empty();
				TreeViewPtr->ClearSelection();
			}
			else
			{
				// Delay clear until first path discovered
				// No clear occurs if selection changes during asset discovery
				bPendingInitialPathsNeedsSelectionClear = true;
			}

			// If the selected paths is empty, the path was "All assets"
			// This should handle that case properly
			for (const FName& Path : NewSelectedPaths)
			{
				if (!ExplicitlyAddPathToSelection(Path))
				{
					// If we could not initially select these paths, but are still discovering assets, add them to a pending list to select them later
					PendingInitialPaths.Add(Path);
				}
			}
		}
		else
		{
			// If all assets are already discovered, just select paths the best we can
			SetSelectedPaths(NewSelectedPaths);
		}
	}

	// Plugin Filters
	if (PluginPathFilters.IsValid())
	{
		TArray<FString> NewSelectedFilters;
		if (FPathViewConfig* PathViewConfig = GetPathViewConfig())
		{
			NewSelectedFilters = PathViewConfig->PluginFilters;
		}
		else
		{
			FString PluginFiltersString;
			if (GConfig->GetString(*IniSection, *(SettingsString + TEXT(".PluginFilters")), PluginFiltersString, IniFilename))
			{
				PluginFiltersString.ParseIntoArray(NewSelectedFilters, TEXT(","), /*bCullEmpty*/ true);
			}
		}

		for (const TSharedRef<FContentBrowserPluginFilter>& Filter : AllPluginPathFilters)
		{
			bool bFilterActive = NewSelectedFilters.Contains(Filter->GetName());
			SetPluginPathFilterActive(Filter, bFilterActive);
		}
	}
}

EActiveTimerReturnType SPathView::SetFocusPostConstruct( double InCurrentTime, float InDeltaTime )
{
	FWidgetPath WidgetToFocusPath;
	FSlateApplication::Get().GeneratePathToWidgetUnchecked( SearchPtr->GetWidget(), WidgetToFocusPath );
	FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );

	return EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType SPathView::TriggerRepopulate(double InCurrentTime, float InDeltaTime)
{
	Populate();
	return EActiveTimerReturnType::Stop;
}

TSharedPtr<SWidget> SPathView::MakePathViewContextMenu()
{
	if (!bAllowContextMenu || !OnGetItemContextMenu.IsBound())
	{
		return nullptr;
	}

	const TArray<FContentBrowserItem> SelectedItems = GetSelectedFolderItems();
	if (SelectedItems.Num() == 0)
	{
		return nullptr;
	}
	
	return OnGetItemContextMenu.Execute(SelectedItems);
}

void SPathView::NewFolderItemRequested(const FContentBrowserItemTemporaryContext& NewItemContext)
{
	bool bAddedTemporaryFolder = false;
	for (const FContentBrowserItemData& NewItemData : NewItemContext.GetItem().GetInternalItems())
	{
		bAddedTemporaryFolder |= AddFolderItem(CopyTemp(NewItemData), /*bUserNamed=*/true).IsValid();
	}

	if (bAddedTemporaryFolder)
	{
		PendingNewFolderContext = NewItemContext;
	}
}

bool SPathView::ExplicitlyAddPathToSelection(const FName Path)
{
	if ( !ensure(TreeViewPtr.IsValid()) )
	{
		return false;
	}

	if (TSharedPtr<FTreeItem> FoundItem = FindTreeItem(Path))
	{
		// Set the selection to the closest found folder and scroll it into view
		RecursiveExpandParents(FoundItem);
		LastSelectedPaths.Add(FoundItem->GetItem().GetInvariantPath());
		TreeViewPtr->SetItemSelection(FoundItem, true);
		TreeViewPtr->RequestScrollIntoView(FoundItem);

		return true;
	}

	return false;
}

bool SPathView::ShouldAllowTreeItemChangedDelegate() const
{
	return PreventTreeItemChangedDelegateCount == 0;
}

void SPathView::RecursiveExpandParents(const TSharedPtr<FTreeItem>& Item)
{
	if ( Item->Parent.IsValid() )
	{
		RecursiveExpandParents(Item->Parent.Pin());
		TreeViewPtr->SetItemExpansion(Item->Parent.Pin(), true);
	}
}

TSharedRef<ITableRow> SPathView::GenerateTreeRow( TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());

	return
		SNew( STableRow< TSharedPtr<FTreeItem> >, OwnerTable )
		.OnDragDetected( this, &SPathView::OnFolderDragDetected )
		[
			SNew(SAssetTreeItem)
			.TreeItem(TreeItem)
			.OnNameChanged(this, &SPathView::FolderNameChanged)
			.OnVerifyNameChanged(this, &SPathView::VerifyFolderNameChanged)
			.IsItemExpanded(this, &SPathView::IsTreeItemExpanded, TreeItem)
			.HighlightText(this, &SPathView::GetHighlightText)
			.IsSelected(this, &SPathView::IsTreeItemSelected, TreeItem)
		];
}

void SPathView::TreeItemScrolledIntoView( TSharedPtr<FTreeItem> TreeItem, const TSharedPtr<ITableRow>& Widget )
{
	if ( TreeItem->IsNamingFolder() && Widget.IsValid() && Widget->GetContent().IsValid() )
	{
		TreeItem->OnRenameRequested().Broadcast();
	}
}

void SPathView::GetChildrenForTree( TSharedPtr< FTreeItem > TreeItem, TArray< TSharedPtr<FTreeItem> >& OutChildren )
{
	TreeItem->SortChildrenIfNeeded();
	OutChildren = TreeItem->Children;
}

void SPathView::SetTreeItemExpansionRecursive( TSharedPtr< FTreeItem > TreeItem, bool bInExpansionState )
{
	TreeViewPtr->SetItemExpansion(TreeItem, bInExpansionState);

	// Recursively go through the children.
	for(auto It = TreeItem->Children.CreateIterator(); It; ++It)
	{
		SetTreeItemExpansionRecursive( *It, bInExpansionState );
	}
}

void SPathView::TreeSelectionChanged( TSharedPtr< FTreeItem > TreeItem, ESelectInfo::Type SelectInfo )
{
	if ( ShouldAllowTreeItemChangedDelegate() )
	{
		const TArray<TSharedPtr<FTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();

		LastSelectedPaths.Empty();
		for (int32 ItemIdx = 0; ItemIdx < SelectedItems.Num(); ++ItemIdx)
		{
			const TSharedPtr<FTreeItem> Item = SelectedItems[ItemIdx];
			if ( !ensure(Item.IsValid()) )
			{
				// All items must exist
				continue;
			}

			// Keep track of the last paths that we broadcasted for selection reasons when filtering
			LastSelectedPaths.Add(Item->GetItem().GetInvariantPath());
		}

		if ( OnItemSelectionChanged.IsBound() )
		{
			if ( TreeItem.IsValid() )
			{
				OnItemSelectionChanged.Execute(TreeItem->GetItem(), SelectInfo);
			}
			else
			{
				OnItemSelectionChanged.Execute(FContentBrowserItem(), SelectInfo);
			}
		}

		if (FPathViewConfig* PathViewConfig = GetPathViewConfig())
		{
			PathViewConfig->SelectedPaths = LastSelectedPaths.Array();

			UContentBrowserConfig::Get()->SaveEditorConfig();
		}
	}

	if (TreeItem.IsValid())
	{
		// Prioritize the content scan for the selected path
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		ContentBrowserData->PrioritizeSearchPath(TreeItem->GetItem().GetVirtualPath());
	}
}

void SPathView::TreeExpansionChanged( TSharedPtr< FTreeItem > TreeItem, bool bIsExpanded )
{
	if ( ShouldAllowTreeItemChangedDelegate() )
	{
		DirtyLastExpandedPaths();

		if (!bIsExpanded)
		{
			const TArray<TSharedPtr<FTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();
			bool bSelectTreeItem = false;

			// If any selected item was a child of the collapsed node, then add the collapsed node to the current selection
			// This avoids the selection ever becoming empty, as this causes the Content Browser to show everything
			for (const TSharedPtr<FTreeItem>& SelectedItem : SelectedItems)
			{
				if (SelectedItem->IsChildOf(*TreeItem.Get()))
				{
					bSelectTreeItem = true;
					break;
				}
			}

			if (bSelectTreeItem)
			{
				TreeViewPtr->SetItemSelection(TreeItem, true);
			}
		}
	}
}

void SPathView::FilterUpdated()
{
	Populate(/*bIsRefreshingFilter*/true);
}

void SPathView::SetSearchFilterText(const FText& InSearchText, TArray<FText>& OutErrors)
{
	SearchBoxFolderFilter->SetRawFilterText(InSearchText);

	const FText ErrorText = SearchBoxFolderFilter->GetFilterErrorText();
	if (!ErrorText.IsEmpty())
	{
		OutErrors.Add(ErrorText);
	}
}

FText SPathView::GetHighlightText() const
{
	return SearchBoxFolderFilter->GetRawFilterText();
}

void SPathView::Populate(const bool bIsRefreshingFilter)
{
	// Update the list of expanded path before removing the items
	UpdateLastExpandedPathsIfDirty();

	const bool bFilteringByText = !SearchBoxFolderFilter->GetRawFilterText().IsEmpty();

	// Batch the selection changed event
	// Only emit events when the user isn't filtering, as the selection may be artificially limited by the filter
	FScopedSelectionChangedEvent ScopedSelectionChangedEvent(SharedThis(this), !bFilteringByText && !bIsRefreshingFilter);

	// Clear all root items and clear selection
	ClearTreeItems();

	// Populate the view
	{
		const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
		bool bDisplayEmpty = ContentBrowserSettings->DisplayEmptyFolders;
		// check to see if we have an instance config that overrides the default in UContentBrowserSettings
		if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
		{
			bDisplayEmpty = EditorConfig->bShowEmptyFolders;
		}

		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		const FContentBrowserDataCompiledFilter CompiledDataFilter = CreateCompiledFolderFilter();

		TArray<TSharedPtr<FTreeItem>> ItemsCreated;
		ContentBrowserData->EnumerateItemsMatchingFilter(CompiledDataFilter, [this, bFilteringByText, bDisplayEmpty, ContentBrowserData, &ItemsCreated](FContentBrowserItemData&& InItemData)
		{
			bool bPassesFilter = ContentBrowserData->IsFolderVisible(InItemData.GetVirtualPath(), ContentBrowserUtils::GetIsFolderVisibleFlags(bDisplayEmpty));
			if (bPassesFilter && bFilteringByText)
			{
				// Use the whole path so we deliberately include any children of matched parents in the filtered list
				const FString PathStr = InItemData.GetVirtualPath().ToString();
				bPassesFilter &= SearchBoxFolderFilter->PassesFilter(PathStr);
			}

			if (bPassesFilter)
			{
				// Using array of all items created to handle item expansion of fully virtual paths that may not be included in enumeration
				ItemsCreated.Reset();
				AddFolderItem(MoveTemp(InItemData), /*bUserNamed=*/ false, &ItemsCreated);

				for (TSharedPtr<FTreeItem> Item : ItemsCreated)
				{
					const FName InvariantPath = Item->GetItem().GetInvariantPath();
					const bool bSelectedItem = LastSelectedPaths.Contains(InvariantPath);
					const bool bExpandedItem = LastExpandedPaths.Contains(InvariantPath);

					if (bFilteringByText || bSelectedItem)
					{
						RecursiveExpandParents(Item);
					}

					if (bSelectedItem)
					{
						// Tree items that match the last broadcasted paths should be re-selected them after they are added
						if (!TreeViewPtr->IsItemSelected(Item))
						{
							TreeViewPtr->SetItemSelection(Item, true);
						}
						TreeViewPtr->RequestScrollIntoView(Item);
					}

					if (bExpandedItem)
					{
						// Tree items that were previously expanded should be re-expanded when repopulating
						if (!TreeViewPtr->IsItemExpanded(Item))
						{
							TreeViewPtr->SetItemExpansion(Item, true);
						}
					}
				}
			}

			return true;
		});
	}

	SortRootItems();
}

void SPathView::DefaultSort(const FTreeItem* InTreeItem, TArray<TSharedPtr<FTreeItem>>& InChildren)
{
	if (InChildren.Num() < 2)
	{
		return;
	}

	static const FString ClassesPrefix = TEXT("Classes_");

	struct FItemSortInfo
	{
		// Name to display
		FString FolderName;
		float Priority;
		int32 SpecialDefaultFolderPriority;
		bool bIsClassesFolder;
		TSharedPtr<FTreeItem> TreeItem;
		// Name to use when comparing "MyPlugin" vs "Classes_MyPlugin", looking up a plugin by name and other situations
		FName ItemNameWithoutClassesPrefix;
	};

	TArray<FItemSortInfo> SortInfoArray;
	SortInfoArray.Reserve(InChildren.Num());

	const TArray<FName>& SpecialSortFolders = IContentBrowserDataModule::Get().GetSubsystem()->GetPathViewSpecialSortFolders();

	// Generate information needed to perform sort
	for (TSharedPtr<FTreeItem>& It : InChildren)
	{
		FItemSortInfo& SortInfo = SortInfoArray.AddDefaulted_GetRef();
		SortInfo.TreeItem = It;

		const FName InvariantPathFName = It->GetItem().GetInvariantPath();
		FNameBuilder InvariantPathBuilder(InvariantPathFName);
		const FStringView InvariantPath(InvariantPathBuilder);

		bool bIsRootInvariantFolder = false;
		if (InvariantPath.Len() > 1)
		{
			FStringView RootInvariantFolder(InvariantPath);
			RootInvariantFolder.RightChopInline(1);
			int32 SecondSlashIndex = INDEX_NONE;
			bIsRootInvariantFolder = !RootInvariantFolder.FindChar(TEXT('/'), SecondSlashIndex);
		}

		SortInfo.FolderName = It->GetItem().GetDisplayName().ToString();

		SortInfo.bIsClassesFolder = false;
		if (bIsRootInvariantFolder)
		{
			FNameBuilder ItemNameBuilder(It->GetItem().GetItemName());
			const FStringView ItemNameView(ItemNameBuilder);
			if (ItemNameView.StartsWith(ClassesPrefix))
			{
				SortInfo.bIsClassesFolder = true;
				SortInfo.ItemNameWithoutClassesPrefix = FName(ItemNameView.RightChop(ClassesPrefix.Len()));
			}

			if (SortInfo.FolderName.StartsWith(ClassesPrefix))
			{
				SortInfo.bIsClassesFolder = true;
				SortInfo.FolderName.RightChopInline(ClassesPrefix.Len(), EAllowShrinking::No);
			}
		}

		if (SortInfo.ItemNameWithoutClassesPrefix.IsNone())
		{
			SortInfo.ItemNameWithoutClassesPrefix = It->GetItem().GetItemName();
		}

		if (SortInfo.bIsClassesFolder)
		{
			// Sort using a path without "Classes_" prefix
			FStringView InvariantWithoutClassesPrefix(InvariantPath);
			InvariantWithoutClassesPrefix.RightChopInline(1);
			if (InvariantWithoutClassesPrefix.StartsWith(ClassesPrefix))
			{
				InvariantWithoutClassesPrefix.RightChopInline(ClassesPrefix.Len());
				FNameBuilder Builder;
				Builder.Append(TEXT("/"));
				Builder.Append(InvariantWithoutClassesPrefix);
				SortInfo.SpecialDefaultFolderPriority = SpecialSortFolders.IndexOfByKey(FName(Builder));
			}
			else
			{
				SortInfo.SpecialDefaultFolderPriority = SpecialSortFolders.IndexOfByKey(InvariantPathFName);
			}
		}
		else
		{
			SortInfo.SpecialDefaultFolderPriority = SpecialSortFolders.IndexOfByKey(InvariantPathFName);
		}

		if (bIsRootInvariantFolder)
		{
			if (SortInfo.SpecialDefaultFolderPriority == INDEX_NONE)
			{
				SortInfo.Priority = FContentBrowserSingleton::Get().GetPluginSettings(SortInfo.ItemNameWithoutClassesPrefix).RootFolderSortPriority;
			}
			else
			{
				SortInfo.Priority = 1.f;
			}
		}
		else
		{
			if (SortInfo.SpecialDefaultFolderPriority != INDEX_NONE)
			{
				SortInfo.Priority = 1.f;
			}
			else
			{
				SortInfo.Priority = 0.f;
			}
		}
	}

	// Perform sort
	SortInfoArray.Sort([](const FItemSortInfo& SortInfoA, const FItemSortInfo& SortInfoB) -> bool
	{
		if (SortInfoA.Priority != SortInfoB.Priority)
		{
			// Not the same priority, use priority to sort
			return SortInfoA.Priority > SortInfoB.Priority;
		}
		else if (SortInfoA.SpecialDefaultFolderPriority != SortInfoB.SpecialDefaultFolderPriority)
		{
			// Special folders use the index to sort. Non special folders are all set to 0.
			return SortInfoA.SpecialDefaultFolderPriority < SortInfoB.SpecialDefaultFolderPriority;
		}
		else
		{
			// If either is a class folder and names without classes prefix are same
			if ((SortInfoA.bIsClassesFolder != SortInfoB.bIsClassesFolder) && (SortInfoA.ItemNameWithoutClassesPrefix == SortInfoB.ItemNameWithoutClassesPrefix))
			{
				return !SortInfoA.bIsClassesFolder;
			}

			// Two non special folders of the same priority, sort alphabetically
			const int32 CompareResult = UE::ComparisonUtility::CompareWithNumericSuffix(SortInfoA.FolderName, SortInfoB.FolderName);
			if (CompareResult != 0)
			{
				return CompareResult < 0;
			}
			else
			{
				// Classes folders have the same name so sort them adjacent but under non-classes
				return !SortInfoA.bIsClassesFolder;
			}
		}
	});

	// Replace with sorted array
	TArray<TSharedPtr<FTreeItem>> NewList;
	NewList.Reserve(SortInfoArray.Num());
	for (const FItemSortInfo& It : SortInfoArray)
	{
		NewList.Add(It.TreeItem);
	}
	InChildren = MoveTemp(NewList);
}

void SPathView::SortRootItems()
{
	if (SortOverride.IsBound())
	{
		SortOverride.Execute(nullptr, TreeRootItems);
	}
	else
	{
		DefaultSort(nullptr, TreeRootItems);
	}

	TreeViewPtr->RequestTreeRefresh();
}

void SPathView::PopulateFolderSearchStrings( const FString& FolderName, OUT TArray< FString >& OutSearchStrings ) const
{
	OutSearchStrings.Add( FolderName );
}

FReply SPathView::OnFolderDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		if (TSharedPtr<FDragDropOperation> DragDropOp = DragDropHandler::CreateDragOperation(GetSelectedFolderItems()))
		{
			return FReply::Handled().BeginDragDrop(DragDropOp.ToSharedRef());
		}
	}

	return FReply::Unhandled();
}

bool SPathView::VerifyFolderNameChanged(const TSharedPtr< FTreeItem >& TreeItem, const FString& ProposedName, FText& OutErrorMessage) const
{
	if (PendingNewFolderContext.IsValid())
	{
		checkf(FContentBrowserItemKey(TreeItem->GetItem()) == FContentBrowserItemKey(PendingNewFolderContext.GetItem()), TEXT("PendingNewFolderContext was still set when attempting to rename a different item!"));

		return PendingNewFolderContext.ValidateItem(ProposedName, &OutErrorMessage);
	}
	else if (!TreeItem->GetItem().GetItemName().ToString().Equals(ProposedName))
	{
		return TreeItem->GetItem().CanRename(&ProposedName, &OutErrorMessage);
	}

	return true;
}

void SPathView::FolderNameChanged( const TSharedPtr< FTreeItem >& TreeItem, const FString& ProposedName, const UE::Slate::FDeprecateVector2DParameter& MessageLocation, const ETextCommit::Type CommitType )
{
	bool bSuccess = false;
	FText ErrorMessage;

	FContentBrowserItem NewItem;
	if (PendingNewFolderContext.IsValid())
	{
		checkf(FContentBrowserItemKey(TreeItem->GetItem()) == FContentBrowserItemKey(PendingNewFolderContext.GetItem()), TEXT("PendingNewFolderContext was still set when attempting to rename a different item!"));

		// Remove the temporary item before we do any work to ensure the new item creation is not prevented
		RemoveFolderItem(TreeItem);

		// Clearing the rename box on a newly created item cancels the entire creation process
		if (CommitType == ETextCommit::OnCleared)
		{
			// We need to select the parent item of this folder, as the folder would have become selected while it was being named
			if (TSharedPtr<FTreeItem> ParentTreeItem = TreeItem->Parent.Pin())
			{
				TreeViewPtr->SetItemSelection(ParentTreeItem, true);
			}
			else
			{
				TreeViewPtr->ClearSelection();
			}
		}
		else
		{
			UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
			FScopedSuppressContentBrowserDataTick TickSuppression(ContentBrowserData);

			if (PendingNewFolderContext.ValidateItem(ProposedName, &ErrorMessage))
			{
				NewItem = PendingNewFolderContext.FinalizeItem(ProposedName, &ErrorMessage);
				if (NewItem.IsValid())
				{
					bSuccess = true;
				}
			}
		}

		PendingNewFolderContext = FContentBrowserItemTemporaryContext();
	}
	else if (CommitType != ETextCommit::OnCleared && !TreeItem->GetItem().GetItemName().ToString().Equals(ProposedName))
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		FScopedSuppressContentBrowserDataTick TickSuppression(ContentBrowserData);

		if (TreeItem->GetItem().CanRename(&ProposedName, &ErrorMessage) && TreeItem->GetItem().Rename(ProposedName, &NewItem))
		{
			bSuccess = true;
		}
	}

	if (bSuccess && NewItem.IsValid())
	{
		// Add result to view
		TSharedPtr<FTreeItem> NewTreeItem;
		for (const FContentBrowserItemData& NewItemData : NewItem.GetInternalItems())
		{
			NewTreeItem = AddFolderItem(CopyTemp(NewItemData));
		}

		// Select the new item
		if (NewTreeItem)
		{
			TreeViewPtr->SetItemSelection(NewTreeItem, true);
			TreeViewPtr->RequestScrollIntoView(NewTreeItem);
		}
	}

	if (!bSuccess && !ErrorMessage.IsEmpty())
	{
		// Display the reason why the folder was invalid
		FSlateRect MessageAnchor(MessageLocation.X, MessageLocation.Y, MessageLocation.X, MessageLocation.Y);
		ContentBrowserUtils::DisplayMessage(ErrorMessage, MessageAnchor, SharedThis(this));
	}
}

bool SPathView::FolderAlreadyExists(const TSharedPtr< FTreeItem >& TreeItem, TSharedPtr< FTreeItem >& ExistingItem)
{
	ExistingItem.Reset();

	if ( TreeItem.IsValid() )
	{
		if ( TreeItem->Parent.IsValid() )
		{
			// This item has a parent, try to find it in its parent's children
			TSharedPtr<FTreeItem> ParentItem = TreeItem->Parent.Pin();

			for ( auto ChildIt = ParentItem->Children.CreateConstIterator(); ChildIt; ++ChildIt )
			{
				const TSharedPtr<FTreeItem>& Child = *ChildIt;
				if ( Child != TreeItem && Child->GetItem().GetItemName() == TreeItem->GetItem().GetItemName() )
				{
					// The item is in its parent already
					ExistingItem = Child;
					break;
				}
			}
		}
		else
		{
			// This item is part of the root set
			for ( auto RootIt = TreeRootItems.CreateConstIterator(); RootIt; ++RootIt )
			{
				const TSharedPtr<FTreeItem>& Root = *RootIt;
				if ( Root != TreeItem && Root->GetItem().GetItemName() == TreeItem->GetItem().GetItemName() )
				{
					// The item is part of the root set already
					ExistingItem = Root;
					break;
				}
			}
		}
	}

	return ExistingItem.IsValid();
}

void SPathView::RemoveFolderItem(const TSharedPtr< FTreeItem >& TreeItem)
{
	if ( TreeItem.IsValid() )
	{
		if ( TreeItem->Parent.IsValid() )
		{
			// Remove this item from it's parent's list
			TreeItem->Parent.Pin()->Children.Remove(TreeItem);
		}
		else
		{
			// This was a root node, remove from the root list
			TreeRootItems.Remove(TreeItem);
		}

		const FName VirtualPath = TreeItem->GetItem().GetVirtualPath();
		ensure(!FindTreeItem(VirtualPath) || (FindTreeItem(VirtualPath) == TreeItem));
		TreeItemLookup.Remove(VirtualPath);

		TreeViewPtr->RequestTreeRefresh();
	}
}

bool SPathView::IsTreeItemExpanded(TSharedPtr<FTreeItem> TreeItem) const
{
	return TreeViewPtr->IsItemExpanded(TreeItem);
}

bool SPathView::IsTreeItemSelected(TSharedPtr<FTreeItem> TreeItem) const
{
	return TreeViewPtr->IsItemSelected(TreeItem);
}

void SPathView::HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems)
{
	if (InUpdatedItems.Num() == 0)
	{
		return;
	}

	const bool bFilteringByText = !SearchBoxFolderFilter->GetRawFilterText().IsEmpty();

	// Batch the selection changed event
	// Only emit events when the user isn't filtering, as the selection may be artificially limited by the filter
	FScopedSelectionChangedEvent ScopedSelectionChangedEvent(SharedThis(this), !bFilteringByText);

	const double HandleItemDataUpdatedStartTime = FPlatformTime::Seconds();

	const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
	bool bDisplayEmpty = ContentBrowserSettings->DisplayEmptyFolders;
	// check to see if we have an instance config that overrides the default in UContentBrowserSettings
	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bDisplayEmpty = EditorConfig->bShowEmptyFolders;
	}

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	// We defer this compilation as it's quite expensive due to being recursive, and not all updates will contain new folders
	bool bHasCompiledDataFilter = false;
	FContentBrowserDataCompiledFilter CompiledDataFilter;
	auto ConditionalCompileFilter = [this, &bHasCompiledDataFilter, &CompiledDataFilter]()
	{
		if (!bHasCompiledDataFilter)
		{
			bHasCompiledDataFilter = true;
			CompiledDataFilter = CreateCompiledFolderFilter();
		}
	};

	auto DoesItemPassFilter = [this, bFilteringByText, bDisplayEmpty, ContentBrowserData, &CompiledDataFilter](const FContentBrowserItemData& InItemData)
	{
		UContentBrowserDataSource* ItemDataSource = InItemData.GetOwnerDataSource();
		if (!ItemDataSource->DoesItemPassFilter(InItemData, CompiledDataFilter))
		{
			return false;
		}

		if (!ContentBrowserData->IsFolderVisible(InItemData.GetVirtualPath(), ContentBrowserUtils::GetIsFolderVisibleFlags(bDisplayEmpty)))
		{
			return false;
		}

		if (bFilteringByText)
		{
			// Use the whole path so we deliberately include any children of matched parents in the filtered list
			const FString PathStr = InItemData.GetVirtualPath().ToString();
			if (!SearchBoxFolderFilter->PassesFilter(PathStr))
			{
				return false;
			}
		}

		return true;
	};

	for (const FContentBrowserItemDataUpdate& ItemDataUpdate : InUpdatedItems)
	{
		const FContentBrowserItemData& ItemDataRef = ItemDataUpdate.GetItemData();
		if (!ItemDataRef.IsFolder())
		{
			continue;
		}

		ConditionalCompileFilter();

		FContentBrowserItemData ItemData = ItemDataRef;
		ItemData.GetOwnerDataSource()->ConvertItemForFilter(ItemData, CompiledDataFilter);

		switch (ItemDataUpdate.GetUpdateType())
		{
		case EContentBrowserItemUpdateType::Added:
			if (DoesItemPassFilter(ItemData))
			{
				AddFolderItem(MoveTemp(ItemData));
			}
			break;

		case EContentBrowserItemUpdateType::Modified:
			if (DoesItemPassFilter(ItemData))
			{
				AddFolderItem(MoveTemp(ItemData));
			}
			else
			{
				RemoveFolderItem(ItemData);
			}
			break;

		case EContentBrowserItemUpdateType::Moved:
		{
			const FContentBrowserItemData OldMinimalItemData(ItemData.GetOwnerDataSource(), ItemData.GetItemType(), ItemDataUpdate.GetPreviousVirtualPath(), NAME_None, FText(), nullptr);
			RemoveFolderItem(OldMinimalItemData);

			if (DoesItemPassFilter(ItemData))
			{
				AddFolderItem(MoveTemp(ItemData));
			}
		}
		break;

		case EContentBrowserItemUpdateType::Removed:
			RemoveFolderItem(ItemData);
			break;

		default:
			checkf(false, TEXT("Unexpected EContentBrowserItemUpdateType!"));
			break;
		}
	}

	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("PathView - HandleItemDataUpdated completed in %0.4f seconds for %d items"), FPlatformTime::Seconds() - HandleItemDataUpdatedStartTime, InUpdatedItems.Num());
}

void SPathView::HandleItemDataRefreshed()
{
	// Populate immediately, as the path view must be up to date for Content Browser selection to work correctly
	// and since it defaults to being hidden, it potentially won't be ticked to run this update latently
	Populate();

	/*
	// The class hierarchy has changed in some way, so we need to refresh our set of paths
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SPathView::TriggerRepopulate));
	*/
}

void SPathView::HandleItemDataDiscoveryComplete()
{
	// If there were any more initial paths, they no longer exist so clear them now.
	PendingInitialPaths.Empty();
	bPendingInitialPathsNeedsSelectionClear = false;
}

bool SPathView::PathIsFilteredFromViewBySearch(const FString& InPath) const
{
	return !SearchBoxFolderFilter->GetRawFilterText().IsEmpty()
		&& !SearchBoxFolderFilter->PassesFilter(InPath)
		&& !FindTreeItem(*InPath);
}

void SPathView::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayEmptyFolders)) ||
		(PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == "DisplayPluginFolders") ||
		(PropertyName == "DisplayL10NFolder") ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, bDisplayContentFolderSuffix)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, bDisplayFriendlyNameForPluginFolders)) ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		const bool bHadSelectedPath = TreeViewPtr->GetNumItemsSelected() > 0;

		// Update our path view so that it can include/exclude the dev folder
		Populate();

		// If folder is no longer visible but we're inside it...
		if (TreeViewPtr->GetNumItemsSelected() == 0 && bHadSelectedPath)
		{
			for (const FName VirtualPath : GetDefaultPathsToSelect())
			{
				if (TSharedPtr<FTreeItem> TreeItemToSelect = FindTreeItem(VirtualPath))
				{
					TreeViewPtr->SetSelection(TreeItemToSelect);
					break;
				}
			}
		}

		// If the dev or engine folder has become visible and we're inside it...
		const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
		bool bDisplayDev = ContentBrowserSettings->GetDisplayDevelopersFolder();
		bool bDisplayEngine = ContentBrowserSettings->GetDisplayEngineFolder();
		bool bDisplayPlugins = ContentBrowserSettings->GetDisplayPluginFolders();
		bool bDisplayL10N = ContentBrowserSettings->GetDisplayL10NFolder();
		// check to see if we have an instance config that overrides the default in UContentBrowserSettings
		if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
		{
			bDisplayDev = EditorConfig->bShowDeveloperContent;
			bDisplayEngine = EditorConfig->bShowEngineContent;
			bDisplayPlugins = EditorConfig->bShowPluginContent;
			bDisplayL10N = EditorConfig->bShowLocalizedContent;
		}

		if (bDisplayDev || bDisplayEngine || bDisplayPlugins || bDisplayL10N)
		{
			const TArray<FContentBrowserItem> NewSelectedItems = GetSelectedFolderItems();
			if (NewSelectedItems.Num() > 0)
			{
				const FContentBrowserItem& NewSelectedItem = NewSelectedItems[0];

				if ((bDisplayDev && ContentBrowserUtils::IsItemDeveloperContent(NewSelectedItem)) ||
					(bDisplayEngine && ContentBrowserUtils::IsItemEngineContent(NewSelectedItem)) ||
					(bDisplayPlugins && ContentBrowserUtils::IsItemPluginContent(NewSelectedItem)) ||
					(bDisplayL10N && ContentBrowserUtils::IsItemLocalizedContent(NewSelectedItem))
					)
				{
					// Refresh the contents
					OnItemSelectionChanged.ExecuteIfBound(NewSelectedItem, ESelectInfo::Direct);
				}
			}
		}
	}
}

TArray<FName> SPathView::GetDefaultPathsToSelect() const
{
	TArray<FName> VirtualPaths;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (!ContentBrowserModule.GetDefaultSelectedPathsDelegate().ExecuteIfBound(VirtualPaths))
	{
		VirtualPaths.Add(IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(TEXT("/Game")));
	}

	return VirtualPaths;
}

TArray<FName> SPathView::GetRootPathItemNames() const
{
	TArray<FName> RootPathItemNames;
	RootPathItemNames.Reserve(TreeRootItems.Num());
	for (const TSharedPtr<FTreeItem>& RootItem : TreeRootItems)
	{
		if (RootItem.IsValid())
		{
			RootPathItemNames.Add(RootItem->GetItem().GetItemName());
		}
	}

	return RootPathItemNames;
}

TArray<FName> SPathView::GetDefaultPathsToExpand() const
{
	TArray<FName> VirtualPaths;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (!ContentBrowserModule.GetDefaultPathsToExpandDelegate().ExecuteIfBound(VirtualPaths))
	{
		VirtualPaths.Add(IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(TEXT("/Game")));
	}

	return VirtualPaths;
}

void SPathView::DirtyLastExpandedPaths()
{
	bLastExpandedPathsDirty = true;
}

void SPathView::UpdateLastExpandedPathsIfDirty()
{
	if (bLastExpandedPathsDirty)
	{
		TSet<TSharedPtr<FTreeItem>> ExpandedItemSet;
		TreeViewPtr->GetExpandedItems(ExpandedItemSet);

		LastExpandedPaths.Empty(ExpandedItemSet.Num());
		for (const TSharedPtr<FTreeItem>& Item : ExpandedItemSet)
		{
			if (!ensure(Item.IsValid()))
			{
				// All items must exist
				continue;
			}

			// Keep track of the last paths that we broadcasted for expansion reasons when filtering
			LastExpandedPaths.Add(Item->GetItem().GetInvariantPath());
		}

		bLastExpandedPathsDirty = false;
	}
}

TSharedRef<SWidget> SPathView::CreateFavoritesView()
{
	return SAssignNew(FavoritesArea, SExpandableArea)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(4.0f, 4.0f))
		.Padding(0.f)
		.AllowAnimatedTransition(false)
		.InitiallyCollapsed(true)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Favorites", "Favorites"))
			.TextStyle(FAppStyle::Get(), "ButtonText")
			.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
		]
		.BodyContent()
		[
			SNew(SFavoritePathView)
			.OnItemSelectionChanged(OnItemSelectionChanged)
			.OnGetItemContextMenu(OnGetItemContextMenu)
			.FocusSearchBoxWhenOpened(false)
			.ShowTreeTitle(false)
			.ShowSeparator(false)
			.AllowClassesFolder(bAllowClassesFolder)
			.AllowReadOnlyFolders(bAllowReadOnlyFolders)
			.AllowContextMenu(bAllowContextMenu)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserFavorites")))
			.ExternalSearch(SearchPtr)
		];
}


void SFavoritePathView::Construct(const FArguments& InArgs)
{
	SAssignNew(TreeViewPtr, STreeView< TSharedPtr<FTreeItem> >)
		.TreeItemsSource(&TreeRootItems)
		.OnGetChildren(this, &SFavoritePathView::GetChildrenForTree)
		.OnGenerateRow(this, &SFavoritePathView::GenerateTreeRow)
		.OnItemScrolledIntoView(this, &SFavoritePathView::TreeItemScrolledIntoView)
		.ItemHeight(18)
		.SelectionMode(InArgs._SelectionMode)
		.OnSelectionChanged(this, &SFavoritePathView::TreeSelectionChanged)
		.OnContextMenuOpening(this, &SFavoritePathView::MakePathViewContextMenu)
		.ClearSelectionOnClick(false);

	// Bind the favorites menu to update after folder changes
	AssetViewUtils::OnFolderPathChanged().AddSP(this, &SFavoritePathView::FixupFavoritesFromExternalChange); 

	OnFavoritesChangedHandle = FContentBrowserSingleton::Get().RegisterOnFavoritesChangedHandler(FSimpleDelegate::CreateSP(this, &SFavoritePathView::Populate, false));

	SPathView::Construct(InArgs);
}

SFavoritePathView::~SFavoritePathView()
{
	FContentBrowserSingleton::Get().UnregisterOnFavoritesChangedDelegate(OnFavoritesChangedHandle);
}

void SFavoritePathView::Populate(const bool bIsRefreshingFilter)
{
	// Don't allow the selection changed delegate to be fired here
	FScopedPreventTreeItemChangedDelegate DelegatePrevention(SharedThis(this));

	// Clear all root items and clear selection
	ClearTreeItems();

	const TArray<FString>& FavoritePaths = ContentBrowserUtils::GetFavoriteFolders();
	if (FavoritePaths.Num() > 0)
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		const FContentBrowserDataCompiledFilter CompiledDataFilter = CreateCompiledFolderFilter();

		for (const FString& InvariantPath : FavoritePaths)
		{
			FName VirtualPath;
			IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(InvariantPath, VirtualPath);
			const FString Path = VirtualPath.ToString();

			// Use the whole path so we deliberately include any children of matched parents in the filtered list
			if (!SearchBoxFolderFilter->PassesFilter(Path))
			{
				continue;
			}

			ContentBrowserData->EnumerateItemsAtPath(*Path, CompiledDataFilter.ItemTypeFilter, 
			[this, &CompiledDataFilter](FContentBrowserItemData&& InItemData)
				{
					UContentBrowserDataSource* ItemDataSource = InItemData.GetOwnerDataSource();
					ItemDataSource->ConvertItemForFilter(InItemData, CompiledDataFilter);
					if (ItemDataSource->DoesItemPassFilter(InItemData, CompiledDataFilter))
					{
						if (TSharedPtr<FTreeItem> Item = AddFolderItem(MoveTemp(InItemData)))
						{
							const bool bSelectedItem = LastSelectedPaths.Contains(Item->GetItem().GetInvariantPath());
							if (bSelectedItem)
							{
								// Tree items that match the last broadcasted paths should be re-selected them after they are added
								TreeViewPtr->SetItemSelection(Item, true);
								TreeViewPtr->RequestScrollIntoView(Item);
							}
						}
					}

					return true;
				}
			);
		}
	}

	SortRootItems();
}

void SFavoritePathView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	SPathView::SaveSettings(IniFilename, IniSection, SettingsString);

	FString FavoritePathsString;
	const TArray<FString>& FavoritePaths = ContentBrowserUtils::GetFavoriteFolders();
	for (const FString& PathIt : FavoritePaths)
	{
		if (FavoritePathsString.Len() > 0)
		{
			FavoritePathsString += TEXT(",");
		}

		FavoritePathsString += PathIt;
	}

	GConfig->SetString(*IniSection, TEXT("FavoritePaths"), *FavoritePathsString, IniFilename);
	GConfig->Flush(false, IniFilename);
}

void SFavoritePathView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	SPathView::LoadSettings(IniFilename, IniSection, SettingsString);

	// We clear the initial selection for the favorite view, as it conflicts with the main paths view and results in a phantomly selected favorite item
	ClearSelection();

	// Favorite Paths
	FString FavoritePathsString;
	TArray<FString> NewFavoritePaths;
	if (GConfig->GetString(*IniSection, TEXT("FavoritePaths"), FavoritePathsString, IniFilename))
	{
		FavoritePathsString.ParseIntoArray(NewFavoritePaths, TEXT(","), /*bCullEmpty*/true);
	}

	if (NewFavoritePaths.Num() > 0)
	{
		// Keep track if we changed at least one source so we know to fire the bulk selection changed delegate later
		bool bAddedAtLeastOnePath = false;
		{
			// If the selected paths is empty, the path was "All assets"
			// This should handle that case properly
			for (const FString& InvariantPath : NewFavoritePaths)
			{
				FStringView InvariantPathView(InvariantPath);
				InvariantPathView.TrimStartAndEndInline();
				if (!InvariantPathView.IsEmpty() && InvariantPathView != TEXT("None"))
				{
					ContentBrowserUtils::AddFavoriteFolder(FContentBrowserItemPath(InvariantPathView, EContentBrowserPathType::Internal));
					bAddedAtLeastOnePath = true;
				}
			}
		}

		if (bAddedAtLeastOnePath)
		{
			Populate();
		}
	}
}

TSharedPtr<FTreeItem> SFavoritePathView::AddFolderItem(FContentBrowserItemData&& InItem, const bool bUserNamed, TArray<TSharedPtr<FTreeItem>>* OutItemsCreated)
{
	if (!ensure(TreeViewPtr.IsValid()))
	{
		// No tree view for some reason
		return nullptr;
	}

	// The favorite view will add all items at the root level

	// Try and find an existing tree item
	TWeakPtr<FTreeItem>* WeakExistingItem = TreeItemLookup.Find(InItem.GetVirtualPath());
	if (WeakExistingItem != nullptr)
	{
		if (TSharedPtr<FTreeItem> ExistingItem = WeakExistingItem->Pin())
		{
			ExistingItem->AppendItemData(InItem);
			return ExistingItem;
		}
	}

	// No match - create a new item
	TSharedPtr<FTreeItem> CurrentTreeItem = MakeShared<FTreeItem>(MoveTemp(InItem));
	TreeRootItems.Add(CurrentTreeItem);
	TreeItemLookup.Add(CurrentTreeItem->GetItem().GetVirtualPath(), CurrentTreeItem);
	//TreeViewPtr->SetSelection(CurrentTreeItem);
	TreeViewPtr->RequestTreeRefresh();
	if (OutItemsCreated)
	{
		OutItemsCreated->Add(CurrentTreeItem);
	}

	return CurrentTreeItem;
}

TSharedRef<ITableRow> SFavoritePathView::GenerateTreeRow(TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(TreeItem.IsValid());

	return
		SNew( STableRow< TSharedPtr<FTreeItem> >, OwnerTable )
		.OnDragDetected( this, &SFavoritePathView::OnFolderDragDetected )
		[
			SNew(SAssetTreeItem)
			.TreeItem(TreeItem)
			.OnNameChanged(this, &SFavoritePathView::FolderNameChanged)
			.OnVerifyNameChanged(this, &SFavoritePathView::VerifyFolderNameChanged)
			.IsItemExpanded(false)
			.HighlightText(this, &SFavoritePathView::GetHighlightText)
			.IsSelected(this, &SFavoritePathView::IsTreeItemSelected, TreeItem)
			.FontOverride(FAppStyle::GetFontStyle("ContentBrowser.SourceTreeItemFont"))
		];
}

void SFavoritePathView::HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems)
{
	if (InUpdatedItems.Num() == 0)
	{
		return;
	}

	TSet<FName> FavoritePaths;
	{
		const TArray<FString>& FavoritePathStrs = ContentBrowserUtils::GetFavoriteFolders();
		for (const FString& InvariantPath : FavoritePathStrs)
		{
			FName VirtualPath;
			IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(InvariantPath, VirtualPath);
			FavoritePaths.Add(VirtualPath);
		}
	}
	if (FavoritePaths.Num() == 0)
	{
		return;
	}

	// Don't allow the selection changed delegate to be fired here
	FScopedPreventTreeItemChangedDelegate DelegatePrevention(SharedThis(this));

	const double HandleItemDataUpdatedStartTime = FPlatformTime::Seconds();

	const bool bFilteringByText = !SearchBoxFolderFilter->GetRawFilterText().IsEmpty();

	// We defer this compilation as it's quite expensive due to being recursive, and not all updates will contain new folders
	bool bHasCompiledDataFilter = false;
	FContentBrowserDataCompiledFilter CompiledDataFilter;
	auto ConditionalCompileFilter = [this, &bHasCompiledDataFilter, &CompiledDataFilter]()
	{
		if (!bHasCompiledDataFilter)
		{
			bHasCompiledDataFilter = true;
			CompiledDataFilter = CreateCompiledFolderFilter();
		}
	};

	auto DoesItemPassFilter = [this, bFilteringByText, &CompiledDataFilter, &FavoritePaths](const FContentBrowserItemData& InItemData)
	{
		if (!FavoritePaths.Contains(InItemData.GetVirtualPath()))
		{
			return false;
		}

		UContentBrowserDataSource* ItemDataSource = InItemData.GetOwnerDataSource();
		if (!ItemDataSource->DoesItemPassFilter(InItemData, CompiledDataFilter))
		{
			return false;
		}

		if (bFilteringByText)
		{
			// Use the whole path so we deliberately include any children of matched parents in the filtered list
			const FString PathStr = InItemData.GetVirtualPath().ToString();
			if (!SearchBoxFolderFilter->PassesFilter(PathStr))
			{
				return false;
			}
		}

		return true;
	};

	for (const FContentBrowserItemDataUpdate& ItemDataUpdate : InUpdatedItems)
	{
		const FContentBrowserItemData& ItemDataRef = ItemDataUpdate.GetItemData();
		if (!ItemDataRef.IsFolder())
		{
			continue;
		}

		ConditionalCompileFilter();

		FContentBrowserItemData ItemData = ItemDataUpdate.GetItemData();
		ItemData.GetOwnerDataSource()->ConvertItemForFilter(ItemData, CompiledDataFilter);

		switch (ItemDataUpdate.GetUpdateType())
		{
		case EContentBrowserItemUpdateType::Added:
			if (DoesItemPassFilter(ItemData))
			{
				AddFolderItem(MoveTemp(ItemData));
			}
			break;

		case EContentBrowserItemUpdateType::Modified:
			if (DoesItemPassFilter(ItemData))
			{
				AddFolderItem(MoveTemp(ItemData));
			}
			else
			{
				RemoveFolderItem(ItemData);
			}
			break;

		case EContentBrowserItemUpdateType::Moved:
		{
			const FContentBrowserItemData OldMinimalItemData(ItemData.GetOwnerDataSource(), ItemData.GetItemType(), ItemDataUpdate.GetPreviousVirtualPath(), NAME_None, FText(), nullptr);
			RemoveFolderItem(OldMinimalItemData);

			if (DoesItemPassFilter(ItemData))
			{
				AddFolderItem(MoveTemp(ItemData));
			}

			ContentBrowserUtils::RemoveFavoriteFolder(FContentBrowserItemPath(ItemDataUpdate.GetPreviousVirtualPath(), EContentBrowserPathType::Virtual));
		}
		break;

		case EContentBrowserItemUpdateType::Removed:
			RemoveFolderItem(ItemData);
			ContentBrowserUtils::RemoveFavoriteFolder(FContentBrowserItemPath(ItemData.GetVirtualPath(), EContentBrowserPathType::Virtual));
			break;

		default:
			checkf(false, TEXT("Unexpected EContentBrowserItemUpdateType!"));
			break;
		}
	}

	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("FavoritePathView - HandleItemDataUpdated completed in %0.4f seconds for %d items"), FPlatformTime::Seconds() - HandleItemDataUpdatedStartTime, InUpdatedItems.Num());
}

bool SFavoritePathView::PathIsFilteredFromViewBySearch(const FString& InPath) const
{
	return SPathView::PathIsFilteredFromViewBySearch(InPath)
		&& ContentBrowserUtils::IsFavoriteFolder(FContentBrowserItemPath(InPath, EContentBrowserPathType::Virtual));
}

void SFavoritePathView::FixupFavoritesFromExternalChange(TArrayView<const AssetViewUtils::FMovedContentFolder> MovedFolders)
{
	for (const AssetViewUtils::FMovedContentFolder& MovedFolder : MovedFolders)
	{
		FContentBrowserItemPath ItemPath(MovedFolder.Key, EContentBrowserPathType::Virtual);
		const bool bWasFavorite = ContentBrowserUtils::IsFavoriteFolder(ItemPath);
		if (bWasFavorite)
		{
			// Remove the original path
			ContentBrowserUtils::RemoveFavoriteFolder(ItemPath);

			// Add the new path to favorites instead
			const FString& NewPath = MovedFolder.Value;
			ContentBrowserUtils::AddFavoriteFolder(FContentBrowserItemPath(NewPath, EContentBrowserPathType::Virtual));
			TSharedPtr<FTreeItem> Item = FindTreeItem(*NewPath);
			if (Item.IsValid())
			{
				TreeViewPtr->SetItemSelection(Item, true);
				TreeViewPtr->RequestScrollIntoView(Item);
			}
		}
	}
	Populate();
}

#undef LOCTEXT_NAMESPACE
