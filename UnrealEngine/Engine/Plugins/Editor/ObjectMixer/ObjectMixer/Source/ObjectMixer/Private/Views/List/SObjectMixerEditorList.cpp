// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/List/SObjectMixerEditorList.h"

// View Filters
#include "Views/List/ObjectMixerEditorListFilters/IObjectMixerEditorListFilter.h"

#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"
#include "ObjectMixerEditorLog.h"
#include "ObjectMixerEditorSettings.h"
#include "ObjectMixerEditorStyle.h"
#include "Views/List/SObjectMixerEditorListRow.h"

#include "ActorBrowsingMode.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/IndexOf.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPlacementModeModule.h"
#include "LevelEditor.h"
#include "LevelEditorMenuContext.h"
#include "LevelEditorViewport.h"
#include "ObjectMixerEditorSerializedData.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Styling/StyleColors.h"
#include "Views/Widgets/ObjectMixerEditorListMenuContext.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "WorldPartition/WorldPartition.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditorList"

const FName SObjectMixerEditorList::ItemNameColumnName(TEXT("Builtin_Name"));
const FName SObjectMixerEditorList::EditorVisibilityColumnName(TEXT("Builtin_EditorVisibility"));
const FName SObjectMixerEditorList::EditorVisibilitySoloColumnName(TEXT("Builtin_EditorVisibilitySolo"));

void SObjectMixerEditorList::Construct(const FArguments& InArgs, TSharedRef<FObjectMixerEditorList> ListModel)
{
	ListModelPtr = ListModel;
	
	// Set Default Sorting info
	ActiveSortingType = EColumnSortMode::Ascending;
	
	HeaderRow = SNew(SHeaderRow)
				.CanSelectGeneratedColumn(false)
				.Visibility(EVisibility::Visible)
				;
	
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]()
			{
				return DoesTreeViewHaveVisibleChildren() ? 0 : 1;
			})
			
			+ SWidgetSwitcher::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(2.0f, 2.0f, 2.0f, 2.0f)
			[
				SAssignNew(TreeViewPtr, STreeView<FObjectMixerEditorListRowPtr>)
				.HeaderRow(HeaderRow)
				.SelectionMode(ESelectionMode::Multi)
				.HighlightParentNodesForSelection(true)
				.OnContextMenuOpening_Lambda([this]()
				{
					TSharedPtr<SWidget> Widget;
					
					if (GetListModelPtr().IsValid())
					{
						SelectedTreeItemsToSelectedInLevelEditor();
						Widget = UObjectMixerEditorListMenuContext::CreateContextMenu(
							{GetSelectedTreeViewItems(), GetListModelPtr().Pin()->GetMainPanelModel()});
					}
					
					return Widget;
				})
				.OnSelectionChanged_Lambda([this] (const FObjectMixerEditorListRowPtr& Row, const ESelectInfo::Type SelectionType)
				{
					if (SelectionType != ESelectInfo::Direct) // Don't call if selected from code
					{
						const bool bSyncSelectionEnabled = GetDefault<UObjectMixerEditorSettings>()->bSyncSelection;
						const bool bIsAltDown = FSlateApplication::Get().GetModifierKeys().IsAltDown();
						const bool bShouldSyncSelection = bSyncSelectionEnabled ? !bIsAltDown : bIsAltDown;
						
						if (bShouldSyncSelection)
						{
							SelectedTreeItemsToSelectedInLevelEditor();
						}
					}
				})
				.OnMouseButtonDoubleClick_Lambda([this] (FObjectMixerEditorListRowPtr Row)
				{							
					if (Row->GetRowType() == FObjectMixerEditorListRow::MatchingObject ||
						Row->GetRowType() == FObjectMixerEditorListRow::ContainerObject)
					{
						if (GCurrentLevelEditingViewportClient)
						{
							AActor* Actor = Cast<AActor>(Row->GetObject());

							if (!Actor)
							{
								Actor = Row->GetObject()->GetTypedOuter<AActor>();
							}

							if (Actor)
							{
								FVector Origin;
								FVector Extents;
								Actor->GetActorBounds(false, Origin, Extents, true);
								GCurrentLevelEditingViewportClient->FocusViewportOnBox(FBox(Origin - Extents, Origin + Extents));
							}
						}
					}
					else
					{
						Row->SetIsTreeViewItemExpanded(!Row->GetIsTreeViewItemExpanded());
					}
				})
				.TreeItemsSource(&VisibleTreeViewObjects)
				.OnGenerateRow_Lambda([this](FObjectMixerEditorListRowPtr Row, const TSharedRef<STableViewBase>& OwnerTable)
					{
						check(Row.IsValid());
					
						return SNew(SObjectMixerEditorListRow, TreeViewPtr.ToSharedRef(), Row)
								.Visibility_Raw(Row.Get(), &FObjectMixerEditorListRow::GetDesiredRowWidgetVisibility);
					})
				.OnGetChildren_Raw(this, &SObjectMixerEditorList::OnGetRowChildren)
				.OnExpansionChanged_Raw(this, &SObjectMixerEditorList::OnRowChildExpansionChange, false)
				.OnSetExpansionRecursive(this, &SObjectMixerEditorList::OnRowChildExpansionChange, true)
			]

			// For when no rows exist in view
			+ SWidgetSwitcher::Slot()
			.HAlign(HAlign_Fill)
			.Padding(2.0f, 24.0f, 2.0f, 2.0f)
			[
				SNew(SRichTextBlock)
				.DecoratorStyleSet(&FAppStyle::Get())
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
				.Text_Lambda([this]()
				{
					// Preset Empty List (with filter)
                    return LOCTEXT("EmptyListPresetWithFilter", "No matching items in your list.\n\nCheck your filters.");
				})
			]
		]
	];
}

SObjectMixerEditorList::~SObjectMixerEditorList()
{	
	HeaderRow.Reset();

	FlushMemory(false);

	TreeViewPtr.Reset();
}

void SObjectMixerEditorList::RebuildList()
{
	bIsRebuildRequested = false;
	
	GenerateTreeView();
}

void SObjectMixerEditorList::RefreshList()
{
	if (TreeViewRootObjects.Num() > 0)
	{
		// Apply last search
		ExecuteListViewSearchOnAllRows(GetSearchStringFromSearchInputField(), false);

		// Enforce Sort
		const FName& SortingName = GetActiveSortingColumnName();
		ExecuteSort(SortingName, GetSortModeForColumn(SortingName), false);

		// Show/Hide rows based on SetBy changes and filter settings
		EvaluateIfRowsPassFilters(false);
	}

	FindVisibleObjectsAndRequestTreeRefresh();
}

void SObjectMixerEditorList::RequestRebuildList(const FString& InItemToScrollTo)
{
	bIsRebuildRequested = true;
}

TArray<FObjectMixerEditorListRowPtr> SObjectMixerEditorList::GetSelectedTreeViewItems() const
{
	return TreeViewPtr->GetSelectedItems();
}

int32 SObjectMixerEditorList::GetSelectedTreeViewItemCount() const
{
	return TreeViewPtr->GetSelectedItems().Num();
}

void SObjectMixerEditorList::SyncEditorSelectionToListSelection()
{
	if (GetDefault<UObjectMixerEditorSettings>()->bSyncSelection && !bShouldPauseSyncSelection)
	{
		bIsEditorToListSelectionSyncRequested = false;
		const USelection* SelectedActors = GEditor->GetSelectedActors();

		if (const int32 SelectionCount = SelectedActors->Num(); SelectionCount > 0)
		{
			bShouldPauseSyncSelection = true;
			TreeViewPtr->ClearSelection();

			FObjectMixerEditorListRowPtr* ListRowPtr = nullptr;
			for (int32 SelectionItr = 0; SelectionItr < SelectionCount; SelectionItr++)
			{
				UObject* SelectedActor = SelectedActors->GetSelectedObject(SelectionItr);

				ListRowPtr = ObjectsToRowsCreated.Find(SelectedActor);

				if (ListRowPtr)
				{
					TreeViewPtr->SetItemSelection(*ListRowPtr, true);
				}
			}
			
			// Scroll to the last selected item
			if (ListRowPtr)
			{
				TreeViewPtr->RequestScrollIntoView(*ListRowPtr);
			}
		}
	}
}

void SObjectMixerEditorList::SetSelectedTreeViewItemActorsEditorVisible(const bool bNewIsVisible, const bool bIsRecursive)
{
	for (const TSharedPtr<FObjectMixerEditorListRow>& SelectedItem : TreeViewPtr->GetSelectedItems())
	{
		SelectedItem->SetObjectVisibility(bNewIsVisible, bIsRecursive);
	}
}

bool SObjectMixerEditorList::IsTreeViewItemSelected(TSharedRef<FObjectMixerEditorListRow> Item)
{
	return TreeViewPtr->GetSelectedItems().Contains(Item);
}

TArray<FObjectMixerEditorListRowPtr> SObjectMixerEditorList::GetTreeViewItems() const
{
	return TreeViewRootObjects;
}

void SObjectMixerEditorList::SetTreeViewItems(const TArray<FObjectMixerEditorListRowPtr>& InItems)
{
	TreeViewRootObjects = InItems;

	TreeViewPtr->RequestListRefresh();
}

TSet<TWeakPtr<FObjectMixerEditorListRow>> SObjectMixerEditorList::GetSoloRows()
{
	return GetListModelPtr().Pin()->GetSoloRows();
}

void SObjectMixerEditorList::AddSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow)
{
	GetListModelPtr().Pin()->AddSoloRow(InRow);
}

void SObjectMixerEditorList::RemoveSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow)
{
	GetListModelPtr().Pin()->RemoveSoloRow(InRow);
}

void SObjectMixerEditorList::ClearSoloRows()
{
	GetListModelPtr().Pin()->ClearSoloRows();
}

FText SObjectMixerEditorList::GetSearchTextFromSearchInputField() const
{
	TSharedPtr<FObjectMixerEditorList> PinnedListModel = ListModelPtr.Pin();
	check(PinnedListModel && PinnedListModel->GetMainPanelModel().IsValid());
	check(HeaderRow);
	
	return PinnedListModel->GetMainPanelModel().Pin()->GetSearchTextFromSearchInputField();
}

FString SObjectMixerEditorList::GetSearchStringFromSearchInputField() const
{
	TSharedPtr<FObjectMixerEditorList> PinnedListModel = ListModelPtr.Pin();
	check(PinnedListModel && PinnedListModel->GetMainPanelModel().IsValid());
	check(HeaderRow);
	
	return PinnedListModel->GetMainPanelModel().Pin()->GetSearchStringFromSearchInputField();
}

void SObjectMixerEditorList::ExecuteListViewSearchOnAllRows(
	const FString& SearchString, const bool bShouldRefreshAfterward)
{
	TArray<FString> Tokens;
	
	// unquoted search equivalent to a match-any-of search
	SearchString.ParseIntoArray(Tokens, TEXT("|"), true);
	
	for (const TSharedPtr<FObjectMixerEditorListRow>& ChildRow : TreeViewRootObjects)
	{
		if (!ensure(ChildRow.IsValid()))
		{
			continue;
		}
		
		const bool bGroupMatch = ChildRow->MatchSearchTokensToSearchTerms(Tokens);
		
		// If the group name matches then we pass in an empty string so all child nodes are visible.
		// If the name doesn't match, then we need to evaluate each child.
		ChildRow->ExecuteSearchOnChildNodes(bGroupMatch ? "" : SearchString);
	}

	if (bShouldRefreshAfterward)
	{
		FindVisibleObjectsAndRequestTreeRefresh();
	}
}

bool SObjectMixerEditorList::DoesTreeViewHaveVisibleChildren() const
{
	if (TreeViewPtr.IsValid())
	{
		for (const TSharedPtr<FObjectMixerEditorListRow>& Header : TreeViewRootObjects)
		{
			const EVisibility HeaderVisibility = Header->GetDesiredRowWidgetVisibility();
			
			if (HeaderVisibility != EVisibility::Hidden && HeaderVisibility != EVisibility::Collapsed)
			{
				return true;
			}
		}
	}
	
	return false;
}

bool SObjectMixerEditorList::IsTreeViewItemExpanded(const TSharedPtr<FObjectMixerEditorListRow>& Row) const
{
	if (TreeViewPtr.IsValid())
	{
		return TreeViewPtr->IsItemExpanded(Row);
	}
	
	return false;
}

void SObjectMixerEditorList::SetTreeViewItemExpanded(const TSharedPtr<FObjectMixerEditorListRow>& RowToExpand, const bool bNewExpansion) const
{
	if (TreeViewPtr.IsValid())
	{
		TreeViewPtr->SetItemExpansion(RowToExpand, bNewExpansion);
	}
}

EObjectMixerTreeViewMode SObjectMixerEditorList::GetTreeViewMode()
{
	const TSharedPtr<FObjectMixerEditorList> PinnedListModel = GetListModelPtr().Pin();
	check(PinnedListModel);

	const TSharedPtr<FObjectMixerEditorMainPanel> PinnedMainPanel = PinnedListModel->GetMainPanelModel().Pin();
	check(PinnedMainPanel);
	
	return PinnedMainPanel->GetTreeViewMode();
}

const TArray<TSharedRef<IObjectMixerEditorListFilter>>& SObjectMixerEditorList::GetListFilters()
{
	const TSharedPtr<FObjectMixerEditorList> PinnedListModel = GetListModelPtr().Pin();
	check(PinnedListModel);

	const TSharedPtr<FObjectMixerEditorMainPanel> PinnedMainPanel = PinnedListModel->GetMainPanelModel().Pin();
	check(PinnedMainPanel);
	
	return PinnedMainPanel->GetListFilters();
}

void SObjectMixerEditorList::EvaluateIfRowsPassFilters(const bool bShouldRefreshAfterward)
{
	// Separate filters by type
	
	TSet<TSharedRef<IObjectMixerEditorListFilter>> MatchAnyOfFilters;
	TSet<TSharedRef<IObjectMixerEditorListFilter>> MatchAllOfFilters;

	for (const TSharedRef<IObjectMixerEditorListFilter>& Filter : GetListFilters())
	{
		if (Filter->GetFilterMatchType() ==
			IObjectMixerEditorListFilter::EObjectMixerEditorListFilterMatchType::MatchAll)
		{
			MatchAllOfFilters.Add(Filter);
		}
		else
		{
			MatchAnyOfFilters.Add(Filter);
		}
	}

	const TFunction<void(const TArray<FObjectMixerEditorListRowPtr>&)> RecursiveRowIterator =
		[&MatchAnyOfFilters, &MatchAllOfFilters, &RecursiveRowIterator](const TArray<FObjectMixerEditorListRowPtr>& InObjects)
	{
		bool bExpandByDefault = true;
		if (const UObjectMixerEditorSettings* Settings = GetDefault<UObjectMixerEditorSettings>())
		{
			bExpandByDefault = Settings->bExpandTreeViewItemsByDefault;
		}
			
		for (const FObjectMixerEditorListRowPtr& Row : InObjects)
		{
			if (Row.IsValid())
			{
				auto FilterProjection = [&Row](const TSharedRef<IObjectMixerEditorListFilter>& Filter)
				{
					return Filter->GetIsFilterActive() ? Filter->DoesItemPassFilter(Row) : true;
				};
			
				const bool bPassesAnyOf = MatchAnyOfFilters.Num() ? Algo::AnyOf(MatchAnyOfFilters, FilterProjection) : true;
				const bool bPassesAllOf = MatchAllOfFilters.Num() ? Algo::AllOf(MatchAllOfFilters, FilterProjection) : true;
		
				Row->SetDoesRowPassFilters(bPassesAnyOf && bPassesAllOf);
				
				if (Row->GetChildCount() > 0)
				{
					RecursiveRowIterator(Row->GetChildRows());
				}
				
				Row->SetIsTreeViewItemExpanded(bExpandByDefault);
			}
		}
	};

	RecursiveRowIterator(TreeViewRootObjects);

	if (bShouldRefreshAfterward)
	{
		FindVisibleObjectsAndRequestTreeRefresh();
	}
}

EColumnSortMode::Type SObjectMixerEditorList::GetSortModeForColumn(FName InColumnName) const
{
	EColumnSortMode::Type ColumnSortMode = EColumnSortMode::None;

	if (GetActiveSortingColumnName().IsEqual(InColumnName))
	{
		ColumnSortMode = ActiveSortingType;
	}

	return ColumnSortMode;
}

void SObjectMixerEditorList::OnSortColumnCalled(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortMode)
{
	ExecuteSort(ColumnName, CycleSortMode(ColumnName));
}

EColumnSortMode::Type SObjectMixerEditorList::CycleSortMode(const FName& InColumnName)
{
	const EColumnSortMode::Type PreviousColumnSortMode = GetSortModeForColumn(InColumnName);
	ActiveSortingType = PreviousColumnSortMode ==
		EColumnSortMode::Ascending ? EColumnSortMode::Descending : EColumnSortMode::Ascending;

	ActiveSortingColumnName = InColumnName;
	return ActiveSortingType;
}

void SObjectMixerEditorList::ExecuteSort(
	const FName& InColumnName, const EColumnSortMode::Type InColumnSortMode, const bool bShouldRefreshAfterward)
{	
	if (bShouldRefreshAfterward)
	{
		FindVisibleObjectsAndRequestTreeRefresh();
	}
}

FListViewColumnInfo* SObjectMixerEditorList::GetColumnInfoByPropertyName(const FName& InPropertyName)
{
	return Algo::FindByPredicate(ListViewColumns,
		[InPropertyName] (const FListViewColumnInfo& ColumnInfo)
		{
			return ColumnInfo.PropertyName.IsEqual(InPropertyName);
		});
}

void SObjectMixerEditorList::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bIsRebuildRequested)
	{
		RebuildList();
	}

	if (bIsEditorToListSelectionSyncRequested)
	{
		SyncEditorSelectionToListSelection();
	}

	if (bShouldPauseSyncSelection)
	{
		bShouldPauseSyncSelection = false;
		bIsEditorToListSelectionSyncRequested = false;
	}
}

TSharedRef<SWidget> SObjectMixerEditorList::GenerateHeaderRowContextMenu() const
{
	FMenuBuilder MenuBuilder(false, nullptr);
	
	MenuBuilder.AddSearchWidget();

	FName LastPropertyCategoryName = NAME_None;

	for (const FListViewColumnInfo& ColumnInfo : ListViewColumns)
	{
		const FName& PropertyCategoryName = ColumnInfo.PropertyCategoryName;

		if (!PropertyCategoryName.IsEqual(LastPropertyCategoryName))
		{
			LastPropertyCategoryName = PropertyCategoryName;
			
			MenuBuilder.EndSection();
            MenuBuilder.BeginSection(LastPropertyCategoryName, FText::FromName(LastPropertyCategoryName));
		}
		
		const FName& PropertyName = ColumnInfo.PropertyName;
		
		const FText Tooltip = ColumnInfo.PropertyRef ?
			ColumnInfo.PropertyRef->GetToolTipText() : ColumnInfo.PropertyDisplayText;

		const bool bCanSelectColumn = ColumnInfo.PropertyType != EListViewColumnType::BuiltIn;

		const FName Hook = ColumnInfo.PropertyType == EListViewColumnType::BuiltIn ? "Builtin" : "GeneratedProperties";
		
		MenuBuilder.AddMenuEntry(
			ColumnInfo.PropertyDisplayText,
			Tooltip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, PropertyName]()
				{
					TSharedPtr<FObjectMixerEditorList> PinnedListModel = ListModelPtr.Pin();
					check(PinnedListModel && PinnedListModel->GetMainPanelModel().IsValid());
					check(HeaderRow);

					const bool bNewColumnEnabled = !HeaderRow->IsColumnVisible(PropertyName);
					
					HeaderRow->SetShowGeneratedColumn(PropertyName, bNewColumnEnabled);

					if (UObjectMixerEditorSerializedData* SerializedData = GetMutableDefault<UObjectMixerEditorSerializedData>())
					{
						if (const TSubclassOf<UObjectMixerObjectFilter> Filter = PinnedListModel->GetMainPanelModel().Pin()->GetObjectFilterClass())
						{
							const FName FilterName = Filter->GetFName();
							SerializedData->SetShouldShowColumn(FilterName, PropertyName, bNewColumnEnabled);
						}
					}
				}),
				FCanExecuteAction::CreateLambda([bCanSelectColumn](){return bCanSelectColumn;}),
				FIsActionChecked::CreateLambda([this, PropertyName]()
				{
					check(HeaderRow);
					
					return HeaderRow->IsColumnVisible(PropertyName);
				})
			),
			Hook,
			EUserInterfaceActionType::ToggleButton
		);
	}

	return MenuBuilder.MakeWidget();
}

bool SObjectMixerEditorList::AddUniquePropertyColumnsToHeaderRow(
	FProperty* Property,
	const bool bForceIncludeProperty,
	const TSet<FName>& PropertySkipList)
{
	if (!ensureAlwaysMsgf(Property, TEXT("%hs: Invalid property passed in. Please ensure only valid properties are passed to this function."), __FUNCTION__))
	{
		return false;
	}

	bool bShouldIncludeProperty = bForceIncludeProperty;

	if (!bShouldIncludeProperty)
	{
		const bool bIsPropertyBlueprintEditable = (Property->GetPropertyFlags() & CPF_Edit) != 0;

		// We don't have a proper way to display these yet
		const bool bDoesPropertyHaveSupportedClass =
			!Property->IsA(FMapProperty::StaticClass()) &&
			!Property->IsA(FArrayProperty::StaticClass()) &&
			!Property->IsA(FSetProperty::StaticClass()) &&
			!Property->IsA(FStructProperty::StaticClass());

		bShouldIncludeProperty = bIsPropertyBlueprintEditable && bDoesPropertyHaveSupportedClass;
	}

	if (bShouldIncludeProperty)
	{
		const bool bIsPropertyExplicitlySkipped =
		   PropertySkipList.Num() && PropertySkipList.Contains(Property->GetFName());

		bShouldIncludeProperty = !bIsPropertyExplicitlySkipped;
	}
	
	if (bShouldIncludeProperty)
	{
		const FName PropertyName = Property->GetFName();
	
		// Ensure no duplicate properties
		if (!Algo::FindByPredicate(ListViewColumns,
				[&PropertyName] (const FListViewColumnInfo& ListViewColumn)
				{
					return ListViewColumn.PropertyName.IsEqual(PropertyName);
				})
			)
		{
			FString PropertyCategoryName = "Generated Properties";
			if (const FString* CategoryMeta = Property->FindMetaData("Category"))
			{
				PropertyCategoryName = *CategoryMeta;
			}
			
			ListViewColumns.Add(
				{
					Property, PropertyName,
					Property->GetDisplayNameText(),
					EListViewColumnType::PropertyGenerated,
					*PropertyCategoryName,
					true, false, false
				}
			);

			return true;
		}
	}

	return false;
}

void SObjectMixerEditorList::AddBuiltinColumnsToHeaderRow()
{
	ListViewColumns.Insert(
		{
			nullptr, ItemNameColumnName,
			LOCTEXT("ItemNameHeaderText", "Name"),
			EListViewColumnType::BuiltIn, "Built-In",
			true, false,
			false, 1.0f, 1.7f
		}, 0
	);
	
	ListViewColumns.Insert(
		{
			nullptr, EditorVisibilitySoloColumnName,
			LOCTEXT("EditorVisibilitySoloColumnNameHeaderText", "Solo"),
			EListViewColumnType::BuiltIn, "Built-In",
			true, false,
			true, 25.0f
		}, 0
	);

	ListViewColumns.Insert(
		{
			nullptr, EditorVisibilityColumnName,
			LOCTEXT("EditorVisibilityColumnNameHeaderText", "Visibility"),
			EListViewColumnType::BuiltIn, "Built-In",
			true, false,
			true, 25.0f
		}, 0
	);
}

TSharedPtr<SHeaderRow> SObjectMixerEditorList::GenerateHeaderRow()
{
	TSharedPtr<FObjectMixerEditorList> PinnedListModel = ListModelPtr.Pin();
	check(PinnedListModel && PinnedListModel->GetMainPanelModel().IsValid());
	check(HeaderRow);

	HeaderRow->ClearColumns();
	ListViewColumns.Empty(ListViewColumns.Num());
	
	TSet<UClass*> SpecifiedClasses =
		PinnedListModel->GetMainPanelModel().Pin()->GetObjectFilter()->GetParentAndChildClassesFromSpecifiedClasses(
			ObjectClassesToFilterCache, PropertyInheritanceInclusionOptionsCache);
	
	for (const UClass* Class : SpecifiedClasses)
	{
		for (TFieldIterator<FProperty> FieldIterator(Class, EFieldIterationFlags::None); FieldIterator; ++FieldIterator)
		{
			if (FProperty* Property = *FieldIterator)
			{
				AddUniquePropertyColumnsToHeaderRow(Property, bShouldIncludeUnsupportedPropertiesCache, ColumnsToExcludeCache);
			}
		}

		// Check Force Added Columns
		for (const FName& PropertyName : ForceAddedColumnsCache)
		{
			if (FProperty* Property = FindFProperty<FProperty>(Class, PropertyName))
			{
				AddUniquePropertyColumnsToHeaderRow(Property, true);
			}
		}
	}

	// Alphabetical sort by Property Name
	ListViewColumns.StableSort([](const FListViewColumnInfo& A, const FListViewColumnInfo& B)
	{
		return A.PropertyDisplayText.ToString() < B.PropertyDisplayText.ToString();
	});

	// Alphabetical sort by Property Category Name
	ListViewColumns.StableSort([](const FListViewColumnInfo& A, const FListViewColumnInfo& B)
	{
		return A.PropertyCategoryName.LexicalLess(B.PropertyCategoryName);
	});

	// Add Built-in Columns to beginning
	AddBuiltinColumnsToHeaderRow();

	// Actually add columns to Header
	{	
		const FText ClickToSortTooltip = LOCTEXT("ClickToSort","Click to sort");

		const TSharedRef<SWidget> HeaderMenuContent = GenerateHeaderRowContextMenu();

		for (const FListViewColumnInfo& ColumnInfo : ListViewColumns)
		{
			const FText Tooltip = ColumnInfo.PropertyRef ? ColumnInfo.PropertyRef->GetToolTipText() :
				ColumnInfo.bCanBeSorted ? ClickToSortTooltip : ColumnInfo.PropertyDisplayText;
		
			SHeaderRow::FColumn::FArguments Column =
				SHeaderRow::Column(ColumnInfo.PropertyName)
				.DefaultLabel(ColumnInfo.PropertyDisplayText)
				.ToolTipText(Tooltip)
				.HAlignHeader(EHorizontalAlignment::HAlign_Left)
				//.bHideHeaderMenuButton(false) todo: uncomment this line when 21478974 is pushed 
			;

			if (ColumnInfo.bUseFixedWidth)
			{
				Column.FixedWidth(ColumnInfo.FixedWidth);
			}
			else
			{
				Column.FillWidth(ColumnInfo.FillWidth);
			}

			if (ColumnInfo.bCanBeSorted)
			{
				Column.SortMode_Raw(this, &SObjectMixerEditorList::GetSortModeForColumn, ColumnInfo.PropertyName);
				Column.OnSort_Raw(this, &SObjectMixerEditorList::OnSortColumnCalled);
			}

			if (ColumnInfo.PropertyType == EListViewColumnType::BuiltIn)
			{
				Column.ShouldGenerateWidget(true);
			}

			if (ColumnInfo.PropertyName.IsEqual(EditorVisibilityColumnName))
			{
				Column.HeaderContent()
				[
					SNew(SBox)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(0.f)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Level.VisibleIcon16x"))
						.ToolTipText(LOCTEXT("VisibilityColumnTooltip", "Visibility"))
					]
				];
			}
			else if (ColumnInfo.PropertyName.IsEqual(EditorVisibilitySoloColumnName))
			{
				Column.HeaderContent()
				[
					SNew(SBox)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(0.f)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FObjectMixerEditorStyle::Get().GetBrush("ObjectMixer.Solo"))
						.ToolTipText(LOCTEXT("SoloColumnTooltip", "Solo"))
					]
				];
			}
			else // Add Column Selection Menu widget to all other columns
			{
				Column.MenuContent()
				[
					HeaderMenuContent
				];
			}
		
			HeaderRow->AddColumn(Column);

			// Figure out if we should show a certain column
			// First check filter rules
			bool bShouldShowColumn = ColumnsToShowByDefaultCache.Contains(ColumnInfo.PropertyName);

			// Then load visible columns from SerializedData
			if (UObjectMixerEditorSerializedData* SerializedData = GetMutableDefault<UObjectMixerEditorSerializedData>())
			{
				if (const TSubclassOf<UObjectMixerObjectFilter> Filter = PinnedListModel->GetMainPanelModel().Pin()->GetObjectFilterClass())
				{
					const FName FilterName = Filter->GetFName();
					if (SerializedData->IsColumnDataSerialized(FilterName, ColumnInfo.PropertyName))
					{
						bShouldShowColumn = SerializedData->ShouldShowColumn(FilterName, ColumnInfo.PropertyName);
					}
				}
			}
		
			HeaderRow->SetShowGeneratedColumn(ColumnInfo.PropertyName, bShouldShowColumn);
		}
	}

	return HeaderRow;
}

void SObjectMixerEditorList::FlushMemory(const bool bShouldKeepMemoryAllocated)
{
	if (bShouldKeepMemoryAllocated)
	{
		ObjectsToRowsCreated.Reset();
		TreeViewRootObjects.Reset();
		VisibleTreeViewObjects.Reset();
	}
	else
	{
		ObjectsToRowsCreated.Empty();
		TreeViewRootObjects.Empty();
		VisibleTreeViewObjects.Empty();
	}
}

void SObjectMixerEditorList::SetAllGroupsCollapsed()
{
	if (TreeViewPtr.IsValid())
	{
		for (const FObjectMixerEditorListRowPtr& RootRow : TreeViewRootObjects)
		{
			if (!RootRow.IsValid())
			{
				continue;
			}
			
			TreeViewPtr->SetItemExpansion(RootRow, false);
			RootRow->SetIsTreeViewItemExpanded(false);
		}
	}
}

void SObjectMixerEditorList::CacheTreeState(const TArray<TWeakPtr<IObjectMixerEditorListFilter>>& InFilterCombo)
{
	struct Local
	{
		static void RecursivelyCacheTreeState(
			const TArray<FObjectMixerEditorListRowPtr>& InObjects,
			TArray<FTreeItemStateCache>* TreeItemStateCache,
			TSharedPtr<STreeView<FObjectMixerEditorListRowPtr>> TreeViewPtr)
		{
			for (const TSharedPtr<FObjectMixerEditorListRow>& TreeViewItem : InObjects)
			{
				UObject* RowObject = TreeViewItem->GetObject();
				const FString RowName =
					TreeViewItem->GetRowType() == FObjectMixerEditorListRow::Folder ?
						TreeViewItem->GetFolderPath().ToString() : TreeViewItem->GetDisplayName().ToString();

				if (!RowName.IsEmpty())
				{
					TreeItemStateCache->Add(
						{
							RowObject ? RowObject->GetUniqueID() : -1,
							RowName,
							TreeViewPtr->IsItemExpanded(TreeViewItem), 
							TreeViewPtr->IsItemSelected(TreeViewItem)
						}
					);
				}

				RecursivelyCacheTreeState(TreeViewItem->GetChildRows(), TreeItemStateCache, TreeViewPtr);
			}
		}
	};

	FFilterComboToStateCaches* Match = Algo::FindByPredicate(
		FilterComboToStateCaches,
		[&InFilterCombo](
		const FFilterComboToStateCaches& Cache)
		{
			return Cache.FilterCombo == InFilterCombo;
		});

	if (Match)
	{
		Local::RecursivelyCacheTreeState(TreeViewRootObjects, &Match->Caches, TreeViewPtr);
	}
	else
	{
		TArray<FTreeItemStateCache> NewCache;
		Local::RecursivelyCacheTreeState(TreeViewRootObjects, &NewCache, TreeViewPtr);
		FilterComboToStateCaches.Add({InFilterCombo, MoveTemp(NewCache)});
	}
}

void SObjectMixerEditorList::RestoreTreeState(const TArray<TWeakPtr<IObjectMixerEditorListFilter>>& InFilterCombo, const bool bFlushCache)
{
	struct Local
	{
		static void RecursivelyRestoreTreeState(
			const TArray<FObjectMixerEditorListRowPtr>& InObjects,
			TArray<FTreeItemStateCache>* TreeItemStateCache,
			TSharedPtr<STreeView<FObjectMixerEditorListRowPtr>> TreeViewPtr,
			const bool bExpandByDefault)
		{
			for (const TSharedPtr<FObjectMixerEditorListRow>& TreeViewItem : InObjects)
			{
				if (const FTreeItemStateCache* StateCachePtr = Algo::FindByPredicate(
					*TreeItemStateCache,
					[TreeViewItem](const FTreeItemStateCache& Other)
					{
						if (Other.UniqueId != -1)
						{
							if (const UObject* RowObject = TreeViewItem->GetObject())
							{
								return Other.UniqueId == RowObject->GetUniqueID();
							}
						}

						const FString RowName =
							TreeViewItem->GetRowType() == FObjectMixerEditorListRow::Folder ?
								TreeViewItem->GetFolderPath().ToString() : TreeViewItem->GetDisplayName().ToString();

						return Other.RowName.Equals(RowName);
					}
				))
				{
					TreeViewPtr->SetItemExpansion(TreeViewItem, StateCachePtr->bIsExpanded);
					TreeViewPtr->SetItemSelection(TreeViewItem, StateCachePtr->bIsSelected);
				}
				else
				{
					TreeViewPtr->SetItemExpansion(TreeViewItem, bExpandByDefault);
				}

				RecursivelyRestoreTreeState(
					TreeViewItem->GetChildRows(), TreeItemStateCache, TreeViewPtr, bExpandByDefault);
			}
		}
	};
	
	bool bExpandByDefault = true;
	if (const UObjectMixerEditorSettings* Settings = GetDefault<UObjectMixerEditorSettings>())
	{
		bExpandByDefault = Settings->bExpandTreeViewItemsByDefault;
	}

	for (int32 CachesItr = FilterComboToStateCaches.Num() - 1; CachesItr >= 0; CachesItr--)
	{
		if (FilterComboToStateCaches[CachesItr].FilterCombo == InFilterCombo)
		{
			Local::RecursivelyRestoreTreeState(
			  TreeViewRootObjects, &FilterComboToStateCaches[CachesItr].Caches, TreeViewPtr, bExpandByDefault);

			if (bFlushCache)
			{
				FilterComboToStateCaches.RemoveAt(CachesItr);
			}

			break;
		}
	}
}

void SObjectMixerEditorList::BuildPerformanceCacheAndGenerateHeaderIfNeeded()
{
	TSharedPtr<FObjectMixerEditorList> PinnedListModel = ListModelPtr.Pin();
	check(PinnedListModel && PinnedListModel->GetMainPanelModel().IsValid());
	
	// If any of the following overrides change, we need to regenerate the header row. Otherwise skip regeneration for performance reasons.
	// GetObjectClassesToFilter, GetColumnsToShowByDefault, GetColumnsToExclude,
	// GetForceAddedColumns, GetObjectMixerPropertyInheritanceInclusionOptions, ShouldIncludeUnsupportedProperties
	bool bNeedToGenerateHeaders = false;
	
	const TObjectPtr<UObjectMixerObjectFilter> SelectedFilter = PinnedListModel->GetMainPanelModel().Pin()->GetObjectFilter();
	if (!SelectedFilter)
	{
		UE_LOG(LogObjectMixerEditor, Display, TEXT("%hs: No classes defined in UObjectMixerObjectFilter class."), __FUNCTION__);
		return;
	}

	if (const TSet<UClass*> ObjectClassesToFilter = SelectedFilter->GetObjectClassesToFilter();
		ObjectClassesToFilter.Num() != ObjectClassesToFilterCache.Num() ||
		ObjectClassesToFilter.Difference(ObjectClassesToFilterCache).Num() > 0 || ObjectClassesToFilterCache.Difference(ObjectClassesToFilter).Num() > 0)
	{
		ObjectClassesToFilterCache = ObjectClassesToFilter;
		if (!bNeedToGenerateHeaders)
		{
			bNeedToGenerateHeaders = true;
		}
	}
	if (const TSet<FName> ColumnsToShowByDefault = SelectedFilter->GetColumnsToShowByDefault();
		ColumnsToShowByDefault.Num() != ColumnsToShowByDefaultCache.Num() ||
		ColumnsToShowByDefault.Difference(ColumnsToShowByDefaultCache).Num() > 0 || ColumnsToShowByDefaultCache.Difference(ColumnsToShowByDefault).Num() > 0)
	{
		ColumnsToShowByDefaultCache = ColumnsToShowByDefault;
		if (!bNeedToGenerateHeaders)
		{
			bNeedToGenerateHeaders = true;
		}
	}
	if (const TSet<FName> ColumnsToExclude = SelectedFilter->GetColumnsToExclude();
		ColumnsToExclude.Num() != ColumnsToExcludeCache.Num() ||
		ColumnsToExclude.Difference(ColumnsToExcludeCache).Num() > 0 || ColumnsToExcludeCache.Difference(ColumnsToExclude).Num() > 0)
	{
		ColumnsToExcludeCache = ColumnsToExclude;
		if (!bNeedToGenerateHeaders)
		{
			bNeedToGenerateHeaders = true;
		}
	}
	if (const TSet<FName> ForceAddedColumns = SelectedFilter->GetForceAddedColumns();
		ForceAddedColumns.Num() != ForceAddedColumnsCache.Num() ||
		ForceAddedColumns.Difference(ForceAddedColumnsCache).Num() > 0 || ForceAddedColumnsCache.Difference(ForceAddedColumns).Num() > 0)
	{
		ForceAddedColumnsCache = ForceAddedColumns;
		if (!bNeedToGenerateHeaders)
		{
			bNeedToGenerateHeaders = true;
		}
	}
	if (const EObjectMixerInheritanceInclusionOptions PropertyInheritanceInclusionOptions = SelectedFilter->GetObjectMixerPropertyInheritanceInclusionOptions(); 
		PropertyInheritanceInclusionOptions != PropertyInheritanceInclusionOptionsCache)
	{
		PropertyInheritanceInclusionOptionsCache = PropertyInheritanceInclusionOptions;
		if (!bNeedToGenerateHeaders)
		{
			bNeedToGenerateHeaders = true;
		}
	}
	if (const bool bShouldIncludeUnsupportedProperties = SelectedFilter->ShouldIncludeUnsupportedProperties(); 
		bShouldIncludeUnsupportedProperties != bShouldIncludeUnsupportedPropertiesCache)
	{
		bShouldIncludeUnsupportedPropertiesCache = bShouldIncludeUnsupportedProperties;
		if (!bNeedToGenerateHeaders)
		{
			bNeedToGenerateHeaders = true;
		}
	}
	
	if (bNeedToGenerateHeaders)
	{
		GenerateHeaderRow();
	}
}

bool DoesValidWorldObjectHaveAcceptableClass(const UObject* Object, const TSet<UClass*>& ObjectClassesToFilterCache)
{
	if (IsValid(Object))
	{
		for (UClass* Class : ObjectClassesToFilterCache)
		{
			if (Object->IsA(Class))
			{
				return true;
			}
		}
	}

	return false;
}

void CreateOuterRowsForTopLevelRow(
	UObject* InObject,
	FObjectMixerEditorListRowPtr& TopLevelRow,
	const TSet<UObject*>& AllMatchingObjects,
	TMap<UObject*, FObjectMixerEditorListRowPtr>& ObjectsWithRowCreated,
	const TSharedRef<SObjectMixerEditorList>& InListView)
{
	if (InObject)
	{
		if (InObject->IsA(ULevel::StaticClass()) ||
			InObject->IsA(UWorld::StaticClass()) ||
			InObject->IsA(UPackage::StaticClass()))
		{
			return;
		}

		FObjectMixerEditorListRowPtr OuterRow = nullptr;
			
		if (const FObjectMixerEditorListRowPtr* Match = ObjectsWithRowCreated.Find(InObject))
		{
			OuterRow = *Match;
		}
		else
		{				
			OuterRow = MakeShared<FObjectMixerEditorListRow>(
				InObject,
				FObjectMixerEditorListRow::MatchingObject,
				InListView);
				
			check(OuterRow.IsValid());
			ObjectsWithRowCreated.Add(InObject, OuterRow);
		}

		if (TopLevelRow.IsValid())
		{
			OuterRow->AddToChildRows(TopLevelRow);
				
			const bool bMatchingContainer = AllMatchingObjects.Contains(InObject);
				
			OuterRow->SetRowType(
				bMatchingContainer ?
				FObjectMixerEditorListRow::MatchingContainerObject :
				FObjectMixerEditorListRow::ContainerObject
			);
		}
		TopLevelRow = OuterRow;
			
		UObject* Outer = nullptr;
		
		if (const AActor* AsActor = Cast<AActor>(InObject))
		{
			Outer = AsActor->GetAttachParentActor();
		}

		if (!Outer)
		{
			Outer = InObject->GetOuter();
		}

		// Recurse with Outer object
		if (Outer)
		{
			CreateOuterRowsForTopLevelRow(
			   Outer, TopLevelRow, AllMatchingObjects,
			   ObjectsWithRowCreated, InListView
		   );
		}
	}
}

void CreateFolderRowsForTopLevelRow(
	FObjectMixerEditorListRowPtr& TopLevelRow,
	TMap<FName, FObjectMixerEditorListRowPtr>& FolderMap,
	const TSharedRef<SObjectMixerEditorList>& InListView)
{
	if (const TObjectPtr<AActor> AsActor = Cast<AActor>(TopLevelRow->GetObject()))
	{
		FFolder BaseActorFolder = AsActor->GetFolder();

		while (!BaseActorFolder.IsNone())
		{
			TSharedPtr<FObjectMixerEditorListRow> FolderRow;
			
			if (const FObjectMixerEditorListRowPtr* Match = FolderMap.Find(BaseActorFolder.GetPath()))
			{
				FolderRow = *Match;
			}
			else
			{
				FolderRow =
					MakeShared<FObjectMixerEditorListRow>(
						BaseActorFolder.GetPath(), FObjectMixerEditorListRow::Folder, InListView,
						FText::FromName(BaseActorFolder.GetLeafName()));

				FolderMap.Add(BaseActorFolder.GetPath(), FolderRow.ToSharedRef());
			}

			if (FolderRow)
			{
				FolderRow->AddToChildRows(TopLevelRow);
									
				TopLevelRow = FolderRow.ToSharedRef();
			}

			BaseActorFolder = BaseActorFolder.GetParent();
		}
	}
}

void SObjectMixerEditorList::GenerateTreeView()
{
	const TSharedPtr<FObjectMixerEditorList> PinnedListModel = ListModelPtr.Pin();
	check(PinnedListModel);
	
	if (!ensure(TreeViewPtr.IsValid()))
	{
		return;
	}

	TArray<TWeakPtr<IObjectMixerEditorListFilter>> FilterCombo = {};

	if (const TSharedPtr<FObjectMixerEditorMainPanel> PinnedMainPanel = PinnedListModel->GetMainPanelModel().Pin())
	{
		FilterCombo = PinnedMainPanel->GetWeakActiveListFiltersSortedByName();
	}

	CacheTreeState(FilterCombo);
	
	FlushMemory(true);

	BuildPerformanceCacheAndGenerateHeaderIfNeeded();

	check(GEditor);
	const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	
	TSet<UObject*> AllMatchingObjects;

	for (const ULevel* Level : EditorWorld->GetLevels())
	{
		if (Level && Level->bIsVisible)
		{
			ForEachObjectWithOuter(Level,
			  [this, &AllMatchingObjects](UObject* Object)
		  {
			  if (DoesValidWorldObjectHaveAcceptableClass(Object, ObjectClassesToFilterCache))
			  {
				  AllMatchingObjects.Add(Object);
			  }
		  });
		}
	}
	
	TMap<FName, FObjectMixerEditorListRowPtr> FolderMap;
	for (UObject* Object : AllMatchingObjects)
	{
		FObjectMixerEditorListRowPtr TopLevelRow = nullptr;

		UObject* NextOuter = Object;

		CreateOuterRowsForTopLevelRow(
			NextOuter, TopLevelRow, AllMatchingObjects,
			ObjectsToRowsCreated, SharedThis(this)
		);
		
		// Now consider folder hierarchy for the top level row's object if desired
		if (GetTreeViewMode() == EObjectMixerTreeViewMode::Folders)
		{
			CreateFolderRowsForTopLevelRow(TopLevelRow, FolderMap, SharedThis(this));
		}

		TreeViewRootObjects.AddUnique(TopLevelRow);
	}

	TreeViewRootObjects.StableSort(SortByTypeThenName);

	if (TSharedPtr<FObjectMixerEditorMainPanel> MainPanel = PinnedListModel->GetMainPanelModel().Pin())
	{
		MainPanel->RebuildCollectionSelector();
	}

	RefreshList();

	RestoreTreeState(FilterCombo);
}

void SObjectMixerEditorList::FindVisibleTreeViewObjects()
{
	VisibleTreeViewObjects.Empty();

	for (const TSharedPtr<FObjectMixerEditorListRow>& Row : TreeViewRootObjects)
	{
		if (Row->ShouldRowWidgetBeVisible())
		{
			VisibleTreeViewObjects.Add(Row);
		}
	}
}

void SObjectMixerEditorList::FindVisibleObjectsAndRequestTreeRefresh()
{
	FindVisibleTreeViewObjects();
	TreeViewPtr->RequestTreeRefresh();
}

void SObjectMixerEditorList::SelectedTreeItemsToSelectedInLevelEditor()
{
	if (GEditor && !bShouldPauseSyncSelection)
	{
		bShouldPauseSyncSelection = true;
		TArray<AActor*> ActorsToSelect;
		const TArray<FObjectMixerEditorListRowPtr>& SelectedTreeItems = TreeViewPtr->GetSelectedItems();
		
		if (SelectedTreeItems.Num() == 0)
		{
			GEditor->SelectNone(true, true, true);
		}
		else
		{
			for (const TSharedPtr<FObjectMixerEditorListRow>& SelectedRow : SelectedTreeItems)
			{
				if (SelectedRow->GetRowType() == FObjectMixerEditorListRow::MatchingObject ||
					SelectedRow->GetRowType() == FObjectMixerEditorListRow::ContainerObject)
				{
					AActor* Actor = Cast<AActor>(SelectedRow->GetObject());

					if (!Actor)
					{
						Actor = SelectedRow->GetObject()->GetTypedOuter<AActor>();
					}

					if (Actor)
					{
						ActorsToSelect.Add(Actor);
					}
				}
			}

			if (ActorsToSelect.Num())
			{
				FScopedTransaction SyncSelection(LOCTEXT("SyncObjectMixerSelectionToScene","Sync Object Mixer Selection To Scene"));
				
				GEditor->SelectNone(true, true, true);

				for (AActor* Actor : ActorsToSelect)
				{
					Actor->Modify();
					GEditor->SelectActor( Actor, true, true, true );
				}
			}
		}
	}
}

void SObjectMixerEditorList::OnGetRowChildren(FObjectMixerEditorListRowPtr Row, TArray<FObjectMixerEditorListRowPtr>& OutChildren) const
{
	if (Row.IsValid())
	{
		OutChildren = Row->GetChildRows();
		
		if (const int32 HybridIndex = Row->GetOrFindHybridRowIndex(); HybridIndex != INDEX_NONE)
		{
			OutChildren.RemoveAt(HybridIndex);
		}

		if (Row->GetShouldExpandAllChildren())
		{
			Row->SetShouldExpandAllChildren(false);
			SetChildExpansionRecursively(Row, true);
		}
		else
		{
			Row->SetShouldExpandAllChildren(false);
		}
	}
}

void SObjectMixerEditorList::OnRowChildExpansionChange(FObjectMixerEditorListRowPtr Row, const bool bIsExpanded, const bool bIsRecursive) const
{
	if (Row.IsValid())
	{
		if (bIsRecursive)
		{
			if (bIsExpanded)
			{
				if (Row->GetRowType() == FObjectMixerEditorListRow::Folder)
				{
					Row->SetShouldExpandAllChildren(true);
				}
			}
			else
			{
				SetChildExpansionRecursively(Row, bIsExpanded);
			}
		}
		
		Row->SetIsTreeViewItemExpanded(bIsExpanded);
	}
}

void SObjectMixerEditorList::SetChildExpansionRecursively(const FObjectMixerEditorListRowPtr& InRow, const bool bNewIsExpanded) const
{
	if (InRow.IsValid())
	{
		for (const FObjectMixerEditorListRowPtr& Child : InRow->GetChildRows())
		{
			TreeViewPtr->SetItemExpansion(Child, bNewIsExpanded);
			Child->SetIsTreeViewItemExpanded(bNewIsExpanded);

			SetChildExpansionRecursively(Child, bNewIsExpanded);
		}
	}
}

#undef LOCTEXT_NAMESPACE
