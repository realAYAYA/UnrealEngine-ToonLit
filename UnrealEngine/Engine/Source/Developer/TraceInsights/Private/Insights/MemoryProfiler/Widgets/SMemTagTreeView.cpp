// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemTagTreeView.h"

#include "DesktopPlatformModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SlateOptMacros.h"
#include "Templates/UniquePtr.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Views/STableViewBase.h"

// Insights
#include "Insights/Common/Stopwatch.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsStyle.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryTag.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryTracker.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagTreeViewColumnFactory.h"
#include "Insights/MemoryProfiler/Widgets/SMemTagTreeViewTooltip.h"
#include "Insights/MemoryProfiler/Widgets/SMemTagTreeViewTableRow.h"
#include "Insights/MemoryProfiler/Widgets/SMemoryProfilerWindow.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"

#define LOCTEXT_NAMESPACE "SMemTagTreeView"

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemTagTreeView::SMemTagTreeView()
	: ProfilerWindowWeakPtr()
	, Table(MakeShared<Insights::FTable>())
	, bExpansionSaved(false)
	, bFilterOutZeroCountMemTags(false)
	, TrackersFilter(uint64(-1))
	, GroupingMode(EMemTagNodeGroupingMode::Flat)
	, AvailableSorters()
	, CurrentSorter(nullptr)
	, ColumnBeingSorted(GetDefaultColumnBeingSorted())
	, ColumnSortMode(GetDefaultColumnSortMode())
	, StatsStartTime(0.0)
	, StatsEndTime(0.0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemTagTreeView::~SMemTagTreeView()
{
	// Remove ourselves from the Insights manager.
	if (FInsightsManager::Get().IsValid())
	{
		FInsightsManager::Get()->GetSessionChangedEvent().RemoveAll(this);
	}

	Session.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMemTagTreeView::Construct(const FArguments& InArgs, TSharedPtr<SMemoryProfilerWindow> InProfilerWindow)
{
	check(InProfilerWindow.IsValid());
	ProfilerWindowWeakPtr = InProfilerWindow;

	SAssignNew(ExternalScrollbar, SScrollBar)
	.AlwaysShowScrollbar(true);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		.Padding(2.0f, 2.0f, 2.0f, 2.0f)
		[
			SNew(SVerticalBox)

			// Search and Filtering
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				ConstructTagsFilteringWidgetArea()
			]

			// Tracker and Grouping
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				ConstructTagsGroupingWidgetArea()
			]

			// Tracks Mini Toolbar
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				ConstructTracksMiniToolbar()
			]
		]

		// Tree view
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f)
			[
				SAssignNew(TreeView, STreeView<FMemTagNodePtr>)
				.ExternalScrollbar(ExternalScrollbar)
				.SelectionMode(ESelectionMode::Multi)
				.TreeItemsSource(&FilteredGroupNodes)
				.OnGetChildren(this, &SMemTagTreeView::TreeView_OnGetChildren)
				.OnGenerateRow(this, &SMemTagTreeView::TreeView_OnGenerateRow)
				.OnSelectionChanged(this, &SMemTagTreeView::TreeView_OnSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SMemTagTreeView::TreeView_OnMouseButtonDoubleClick)
				.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &SMemTagTreeView::TreeView_GetMenuContent))
				.ItemHeight(16.0f)
				.HeaderRow
				(
					SAssignNew(TreeViewHeaderRow, SHeaderRow)
					.Visibility(EVisibility::Visible)
				)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f)
			[
				SNew(SBox)
				.WidthOverride(FOptionalSize(13.0f))
				[
					ExternalScrollbar.ToSharedRef()
				]
			]
		]
	];

	InitializeAndShowHeaderColumns();

	// Create the search filters: text based, type based etc.
	TextFilter = MakeShared<FMemTagNodeTextFilter>(FMemTagNodeTextFilter::FItemToStringArray::CreateSP(this, &SMemTagTreeView::HandleItemToStringArray));
	Filters = MakeShared<FMemTagNodeFilterCollection>();
	Filters->Add(TextFilter);

	CreateGroupByOptionsSources();
	CreateSortings();

	// Register ourselves with the Insights manager.
	FInsightsManager::Get()->GetSessionChangedEvent().AddSP(this, &SMemTagTreeView::InsightsManager_OnSessionChanged);

	// Update the Session (i.e. when analysis session was already started).
	InsightsManager_OnSessionChanged();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeView::MakeTrackersMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.BeginSection("Trackers"/*, LOCTEXT("ContextMenu_Section_Trackers", "Trackers")*/);
	CreateTrackersMenuSection(MenuBuilder);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateTrackersMenuSection(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		const TArray<TSharedPtr<Insights::FMemoryTracker>>& Trackers = SharedState.GetTrackers();
		for (const TSharedPtr<Insights::FMemoryTracker>& Tracker : Trackers)
		{
			const Insights::FMemoryTrackerId TrackerId = Tracker->GetId();
			MenuBuilder.AddMenuEntry(
				FText::FromString(Tracker->GetName()),
				TAttribute<FText>(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SMemTagTreeView::ToggleTracker, TrackerId),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SMemTagTreeView::IsTrackerChecked, TrackerId)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ToggleTracker(Insights::FMemoryTrackerId InTrackerId)
{
	TrackersFilter ^= Insights::FMemoryTracker::AsFlag(InTrackerId);
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::IsTrackerChecked(Insights::FMemoryTrackerId InTrackerId) const
{
	return (TrackersFilter & Insights::FMemoryTracker::AsFlag(InTrackerId)) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeView::ConstructTagsFilteringWidgetArea()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FInsightsStyle::Get(), "SecondaryToolbar");

	ToolbarBuilder.BeginSection("Filters");
	{
		// Text Filter (Search Box)
		//ToolbarBuilder.AddWidget(
		//	SAssignNew(SearchBox, SSearchBox)
		//	.HintText(LOCTEXT("SearchBoxHint", "Search LLM tags or groups"))
		//	.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search LLM tags or groups"))
		//	.OnTextChanged(this, &SMemTagTreeView::SearchBox_OnTextChanged)
		//	.IsEnabled(this, &SMemTagTreeView::SearchBox_IsEnabled)
		//);

		// Filter Trackers
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &SMemTagTreeView::MakeTrackersMenu),
			LOCTEXT("TrackersMenuText", "Trackers"),
			LOCTEXT("TrackersMenuToolTip", "Filter list of LLM tags by tracker."),
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.Filter.ToolBar"),
			false
		);
	}
	ToolbarBuilder.EndSection();

	TSharedRef<SWidget> Widget = SNew(SHorizontalBox)

	// Search box
	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.Padding(2.0f)
	.FillWidth(1.0f)
	[
		SAssignNew(SearchBox, SSearchBox)
		.HintText(LOCTEXT("SearchBoxHint", "Search LLM tags or groups"))
		.OnTextChanged(this, &SMemTagTreeView::SearchBox_OnTextChanged)
		.IsEnabled(this, &SMemTagTreeView::SearchBox_IsEnabled)
		.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search LLM tags or groups"))
	]

	// Filter out LLM tags with zero instance count.
	//+ SHorizontalBox::Slot()
	//.VAlign(VAlign_Center)
	//.Padding(2.0f)
	//.AutoWidth()
	//[
	//	SNew(SCheckBox)
	//	.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
	//	.HAlign(HAlign_Center)
	//	.Padding(3.0f)
	//	.OnCheckStateChanged(this, &SMemTagTreeView::FilterOutZeroCountMemTags_OnCheckStateChanged)
	//	.IsChecked(this, &SMemTagTreeView::FilterOutZeroCountMemTags_IsChecked)
	//	.ToolTipText(LOCTEXT("FilterOutZeroCountMemTags_Tooltip", "Filter out the LLM tags having zero total instance count (aggregated stats)."))
	//	[
	//		SNew(SImage)
	//		.Image(FInsightsStyle::Get().GetBrush("Icons.ZeroCountFilter"))
	//	]
	//]

	// Filter LLM tags by tracker.
	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.Padding(FMargin(-2.0f, -5.0f))
	.AutoWidth()
	[
		ToolbarBuilder.MakeWidget()
	];

	return Widget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeView::ConstructTagsGroupingWidgetArea()
{
	TSharedRef<SWidget> Widget = SNew(SHorizontalBox)

	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(0.0f, 0.0f, 4.0f, 0.0f)
	[
		SNew(STextBlock)
		.MinDesiredWidth(60.0f)
		.Justification(ETextJustify::Right)
		.Text(LOCTEXT("GroupByText", "Group by"))
	]

	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.MinDesiredWidth(100.0f)
		[
			SAssignNew(GroupByComboBox, SComboBox<TSharedPtr<EMemTagNodeGroupingMode>>)
			.ToolTipText(this, &SMemTagTreeView::GroupBy_GetSelectedTooltipText)
			.OptionsSource(&GroupByOptionsSource)
			.OnSelectionChanged(this, &SMemTagTreeView::GroupBy_OnSelectionChanged)
			.OnGenerateWidget(this, &SMemTagTreeView::GroupBy_OnGenerateWidget)
			[
				SNew(STextBlock)
				.Text(this, &SMemTagTreeView::GroupBy_GetSelectedText)
			]
		]
	];

	return Widget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeView::ConstructTracksMiniToolbar()
{
	TSharedRef<SWidget> Widget = SNew(SHorizontalBox)

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
	.AutoWidth()
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("HideAll_ToolTip", "Remove memory graph tracks for all LLM tags."))
		.OnClicked(this, &SMemTagTreeView::HideAllTracks_OnClicked)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.ContentPadding(FMargin(2.0f, 2.0f, 2.0f, 2.0f))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.5f, 0.5f, 1.0f)))
				.Image(FInsightsStyle::Get().GetBrush("Icons.RemoveAllMemTagGraphs"))
			]
		]
	]

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
	.AutoWidth()
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("ShowAll_ToolTip", "Create memory graph tracks for all visible (filtered) LLM tags."))
		.OnClicked(this, &SMemTagTreeView::ShowAllTracks_OnClicked)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.ContentPadding(FMargin(2.0f, 2.0f, 2.0f, 2.0f))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.0f, 0.5f, 1.0f)))
				.Image(FInsightsStyle::Get().GetBrush("Icons.AddAllMemTagGraphs"))
			]
		]
	]

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
	.AutoWidth()
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("LoadReportXML_ToolTip", "Load LLMReportTypes.xml"))
		.OnClicked(this, &SMemTagTreeView::LoadReportXML_OnClicked)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.ContentPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::Get().GetBrush("Icons.FolderOpen"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle< FTextBlockStyle >(TEXT("ButtonText")))
				.Justification(ETextJustify::Center)
				.Text(LOCTEXT("LoadReportXML_Text", "Load XML..."))
			]
		]
	]

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Right)
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
	[
		SNew(SSegmentedControl<uint32>)
		.OnValueChanged_Lambda([this](uint32 InValue)
			{
				TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
				if (ProfilerWindow.IsValid())
				{
					FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
					SharedState.SetTrackHeightMode((EMemoryTrackHeightMode)InValue);
				}
			})
		.Value_Lambda([this]
			{
				TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
				if (ProfilerWindow.IsValid())
				{
					FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
					return (uint32)SharedState.GetTrackHeightMode();
				}
				return (uint32)EMemoryTrackHeightMode::Medium;
			})
		+ SSegmentedControl<uint32>::Slot((uint32)EMemoryTrackHeightMode::Small)
		.Icon(FInsightsStyle::GetBrush("Icons.SizeSmall"))
		.ToolTip(LOCTEXT("SmallHeight_ToolTip", "Change height of LLM Tag Graph tracks to Small."))
		+ SSegmentedControl<uint32>::Slot((uint32)EMemoryTrackHeightMode::Medium)
		.Icon(FInsightsStyle::GetBrush("Icons.SizeMedium"))
		.ToolTip(LOCTEXT("MediumHeight_ToolTip", "Change height of LLM Tag Graph tracks to Medium."))
		+ SSegmentedControl<uint32>::Slot((uint32)EMemoryTrackHeightMode::Large)
		.Icon(FInsightsStyle::GetBrush("Icons.SizeLarge"))
		.ToolTip(LOCTEXT("LargeHeight_ToolTip", "Change height of LLM Tag Graph tracks to Large."))
	];

	return Widget;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> SMemTagTreeView::TreeView_GetMenuContent()
{
	const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	const int32 NumSelectedNodes = SelectedNodes.Num();
	FMemTagNodePtr SelectedNode = NumSelectedNodes ? SelectedNodes[0] : nullptr;

	FText SelectionStr;
	if (NumSelectedNodes == 0)
	{
		SelectionStr = LOCTEXT("NothingSelected", "Nothing selected");
	}
	else if (NumSelectedNodes == 1)
	{
		FString ItemName = SelectedNode->GetName().ToString();
		const int32 MaxStringLen = 64;
		if (ItemName.Len() > MaxStringLen)
		{
			ItemName = ItemName.Left(MaxStringLen) + TEXT("...");
		}
		SelectionStr = FText::FromString(ItemName);
	}
	else
	{
		SelectionStr = FText::Format(LOCTEXT("MultipleSelection_Fmt", "{0} selected items"), FText::AsNumber(NumSelectedNodes));
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	// Selection menu
	MenuBuilder.BeginSection("Selection", LOCTEXT("ContextMenu_Section_Selection", "Selection"));
	{
		struct FLocal
		{
			static bool ReturnFalse()
			{
				return false;
			}
		};

		FUIAction DummyUIAction;
		DummyUIAction.CanExecuteAction = FCanExecuteAction::CreateStatic(&FLocal::ReturnFalse);
		MenuBuilder.AddMenuEntry
		(
			SelectionStr,
			LOCTEXT("ContextMenu_Selection", "Currently selected items"),
			FSlateIcon(),
			DummyUIAction,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Misc", LOCTEXT("ContextMenu_Section_Misc", "Miscellaneous"));
	{
		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_CreateGraphTracks_SubMenu", "Create Graph Tracks"),
			LOCTEXT("ContextMenu_CreateGraphTracks_SubMenu_Desc", "Create memory graph tracks."),
			FNewMenuDelegate::CreateSP(this, &SMemTagTreeView::TreeView_BuildCreateGraphTracksMenu),
			false,
			FSlateIcon()
		);

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_RemoveGraphTracks_SubMenu", "Remove Graph Tracks"),
			LOCTEXT("ContextMenu_RemoveGraphTracks_SubMenu_Desc", "Remove memory graph tracks."),
			FNewMenuDelegate::CreateSP(this, &SMemTagTreeView::TreeView_BuildRemoveGraphTracksMenu),
			false,
			FSlateIcon()
		);

		FUIAction Action_GenerateColorForSelectedMemTags
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::GenerateColorForSelectedMemTags),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanGenerateColorForSelectedMemTags)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_GenerateColorForSelectedMemTags", "Generate New Color"),
			LOCTEXT("ContextMenu_GenerateColorForSelectedMemTags_Desc", "Generate new color for selected LLM tag(s)."),
			FSlateIcon(),
			Action_GenerateColorForSelectedMemTags,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		FUIAction Action_EditColorForSelectedMemTags
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::EditColorForSelectedMemTags),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanEditColorForSelectedMemTags)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_EditColorForSelectedMemTags", "Edit Color..."),
			LOCTEXT("ContextMenu_EditColorForSelectedMemTags_Desc", "Edit color for selected LLM tag(s)."),
			FSlateIcon(),
			Action_EditColorForSelectedMemTags,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_BuildCreateGraphTracksMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("CreateGraphTracks");
	{
		// Create memory graph tracks for selected LLM tag(s)
		FUIAction Action_CreateGraphTracksForSelectedMemTags
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::CreateGraphTracksForSelectedMemTags),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanCreateGraphTracksForSelectedMemTags)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_CreateGraphTracksForSelectedMemTags", "Selected"),
			LOCTEXT("ContextMenu_CreateGraphTracksForSelectedMemTags_Desc", "Create memory graph tracks for selected LLM tag(s)."),
			FSlateIcon(),
			Action_CreateGraphTracksForSelectedMemTags,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		// Create memory graph tracks for filtered LLM tags
		FUIAction Action_CreateGraphTracksForFilteredMemTags
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::CreateGraphTracksForFilteredMemTags),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanCreateGraphTracksForFilteredMemTags)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_CreateGraphTracksForFilteredMemTags", "Filtered"),
			LOCTEXT("ContextMenu_CreateGraphTracksForFilteredMemTags_Desc", "Create memory graph tracks for all visible (filtered) LLM tags."),
			FSlateIcon(),
			Action_CreateGraphTracksForFilteredMemTags,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		// Create memory graph tracks for all LLM tags
		FUIAction Action_CreateAllGraphTracks
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::CreateAllGraphTracks),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanCreateAllGraphTracks)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_CreateAllGraphTracks", "All"),
			LOCTEXT("ContextMenu_CreateAllGraphTracks_Desc", "Create memory graph tracks for all LLM tags."),
			FSlateIcon(),
			Action_CreateAllGraphTracks,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_BuildRemoveGraphTracksMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("RemoveGraphTracks");
	{
		// Remove memory graph tracks for selected LLM tag(s)
		FUIAction Action_RemoveGraphTracksForSelectedMemTags
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::RemoveGraphTracksForSelectedMemTags),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanRemoveGraphTracksForSelectedMemTags)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_RemoveGraphTracksForSelectedMemTags", "Selected"),
			LOCTEXT("ContextMenu_RemoveGraphTracksForSelectedMemTags_Desc", "Remove memory graph tracks for selected LLM tag(s)."),
			FSlateIcon(),
			Action_RemoveGraphTracksForSelectedMemTags,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		// Remove memory graph tracks for all LLM tags
		FUIAction Action_RemoveAllGraphTracks
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::RemoveAllGraphTracks),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanRemoveAllGraphTracks)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_RemoveAllGraphTracks", "All"),
			LOCTEXT("ContextMenu_RemoveAllGraphTracks_Desc", "Remove memory graph tracks for all LLM tags."),
			FSlateIcon(),
			Action_RemoveAllGraphTracks,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_BuildSortByMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("SortColumn", LOCTEXT("ContextMenu_Section_SortColumn", "Sort Column"));

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (Column.IsVisible() && Column.CanBeSorted())
		{
			FUIAction Action_SortByColumn
			(
				FExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_SortByColumn_Execute, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_SortByColumn_CanExecute, Column.GetId()),
				FIsActionChecked::CreateSP(this, &SMemTagTreeView::ContextMenu_SortByColumn_IsChecked, Column.GetId())
			);
			MenuBuilder.AddMenuEntry
			(
				Column.GetTitleName(),
				Column.GetDescription(),
				FSlateIcon(),
				Action_SortByColumn,
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}
	}

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("SortMode", LOCTEXT("ContextMenu_Section_SortMode", "Sort Mode"));
	{
		FUIAction Action_SortAscending
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_SortMode_Execute, EColumnSortMode::Ascending),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Ascending),
			FIsActionChecked::CreateSP(this, &SMemTagTreeView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Ascending)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_SortAscending", "Sort Ascending"),
			LOCTEXT("ContextMenu_SortAscending_Desc", "Sorts ascending."),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortUp"),
			Action_SortAscending,
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		FUIAction Action_SortDescending
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_SortMode_Execute, EColumnSortMode::Descending),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Descending),
			FIsActionChecked::CreateSP(this, &SMemTagTreeView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Descending)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_SortDescending", "Sort Descending"),
			LOCTEXT("ContextMenu_SortDescending_Desc", "Sorts descending."),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortDown"),
			Action_SortDescending,
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_BuildViewColumnMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Columns", LOCTEXT("ContextMenu_Section_Columns", "Columns"));

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		FUIAction Action_ToggleColumn
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::ToggleColumnVisibility, Column.GetId()),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanToggleColumnVisibility, Column.GetId()),
			FIsActionChecked::CreateSP(this, &SMemTagTreeView::IsColumnVisible, Column.GetId())
		);
		MenuBuilder.AddMenuEntry
		(
			Column.GetTitleName(),
			Column.GetDescription(),
			FSlateIcon(),
			Action_ToggleColumn,
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}

	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::InitializeAndShowHeaderColumns()
{
	// Create columns.
	TArray<TSharedRef<Insights::FTableColumn>> Columns;
	FMemTagTreeViewColumnFactory::CreateMemTagTreeViewColumns(Columns);
	Table->SetColumns(Columns);

	// Show columns.
	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		if (ColumnRef->ShouldBeVisible())
		{
			ShowColumn(ColumnRef->GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemTagTreeView::GetColumnHeaderText(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.GetShortName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeView::TreeViewHeaderRow_GenerateColumnMenu(const Insights::FTableColumn& Column)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("Sorting", LOCTEXT("ContextMenu_Section_Sorting", "Sorting"));
	{
		if (Column.CanBeSorted())
		{
			FUIAction Action_SortAscending
			(
				FExecuteAction::CreateSP(this, &SMemTagTreeView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Ascending),
				FCanExecuteAction::CreateSP(this, &SMemTagTreeView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Ascending),
				FIsActionChecked::CreateSP(this, &SMemTagTreeView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Ascending)
			);
			MenuBuilder.AddMenuEntry
			(
				FText::Format(LOCTEXT("ContextMenu_SortAscending_Fmt", "Sort Ascending (by {0})"), Column.GetTitleName()),
				FText::Format(LOCTEXT("ContextMenu_SortAscending_Desc_Fmt", "Sorts ascending by {0}."), Column.GetTitleName()),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortUp"),
				Action_SortAscending,
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);

				FUIAction Action_SortDescending
				(
					FExecuteAction::CreateSP(this, &SMemTagTreeView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Descending),
					FCanExecuteAction::CreateSP(this, &SMemTagTreeView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Descending),
					FIsActionChecked::CreateSP(this, &SMemTagTreeView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Descending)
				);
				MenuBuilder.AddMenuEntry
				(
				FText::Format(LOCTEXT("ContextMenu_SortDescending_Fmt", "Sort Descending (by {0})"), Column.GetTitleName()),
				FText::Format(LOCTEXT("ContextMenu_SortDescending_Desc_Fmt", "Sorts descending by {0}."), Column.GetTitleName()),
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortDown"),
					Action_SortDescending,
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}

			MenuBuilder.AddSubMenu
			(
				LOCTEXT("ContextMenu_SortBy_SubMenu", "Sort By"),
				LOCTEXT("ContextMenu_SortBy_SubMenu_Desc", "Sorts by a column."),
				FNewMenuDelegate::CreateSP(this, &SMemTagTreeView::TreeView_BuildSortByMenu),
				false,
				FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.SortBy")
			);
		}
		MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ColumnVisibility", LOCTEXT("ContextMenu_Section_ColumnVisibility", "Column Visibility"));
	{
		if (Column.CanBeHidden())
		{
			FUIAction Action_HideColumn
			(
				FExecuteAction::CreateSP(this, &SMemTagTreeView::HideColumn, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &SMemTagTreeView::CanHideColumn, Column.GetId())
			);
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ContextMenu_HideColumn", "Hide"),
				LOCTEXT("ContextMenu_HideColumn_Desc", "Hides the selected column."),
				FSlateIcon(),
				Action_HideColumn,
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_ViewColumn_SubMenu", "View Column"),
			LOCTEXT("ContextMenu_ViewColumn_SubMenu_Desc", "Hides or shows columns."),
			FNewMenuDelegate::CreateSP(this, &SMemTagTreeView::TreeView_BuildViewColumnMenu),
			false,
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ViewColumn")
		);

		FUIAction Action_ShowAllColumns
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_ShowAllColumns_Execute),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_ShowAllColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_ShowAllColumns", "Show All Columns"),
			LOCTEXT("ContextMenu_ShowAllColumns_Desc", "Resets tree view to show all columns."),
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ResetColumn"),
			Action_ShowAllColumns,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		//FUIAction Action_ShowMinMaxMedColumns
		//(
		//	FExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_ShowMinMaxMedColumns_Execute),
		//	FCanExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_ShowMinMaxMedColumns_CanExecute)
		//);
		//MenuBuilder.AddMenuEntry
		//(
		//	LOCTEXT("ContextMenu_ShowMinMaxMedColumns", "Reset Columns to Min/Max/Median Preset"),
		//	LOCTEXT("ContextMenu_ShowMinMaxMedColumns_Desc", "Resets columns to Min/Max/Median preset."),
		//	FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ResetColumn"),
		//	Action_ShowMinMaxMedColumns,
		//	NAME_None,
		//	EUserInterfaceActionType::Button
		//);

		FUIAction Action_ResetColumns
		(
			FExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_ResetColumns_Execute),
			FCanExecuteAction::CreateSP(this, &SMemTagTreeView::ContextMenu_ResetColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_ResetColumns", "Reset Columns to Default"),
			LOCTEXT("ContextMenu_ResetColumns_Desc", "Resets columns to default."),
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ResetColumn"),
			Action_ResetColumns,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::InsightsManager_OnSessionChanged()
{
	TSharedPtr<const TraceServices::IAnalysisSession> NewSession = FInsightsManager::Get()->GetSession();
	if (NewSession != Session)
	{
		Session = NewSession;
		Reset();
	}
	else
	{
		UpdateTree();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::UpdateTree()
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	CreateGroups();

	Stopwatch.Update();
	const double Time1 = Stopwatch.GetAccumulatedTime();

	SortTreeNodes();

	Stopwatch.Update();
	const double Time2 = Stopwatch.GetAccumulatedTime();

	ApplyFiltering();

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(MemoryProfiler, Log, TEXT("[LLM Tags] Tree view updated in %.3fs (%d counters) --> G:%.3fs + S:%.3fs + F:%.3fs"),
			TotalTime, MemTagNodes.Num(), Time1, Time2 - Time1, TotalTime - Time2);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ApplyFiltering()
{
	FilteredGroupNodes.Reset();

	// Apply filter to all groups and its children.
	const int32 NumGroups = GroupNodes.Num();
	for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
	{
		FMemTagNodePtr& GroupPtr = GroupNodes[GroupIndex];
		GroupPtr->ClearFilteredChildren();

		const bool bIsGroupVisible = Filters->PassesAllFilters(GroupPtr);
		int32 NumVisibleChildren = 0;

		const TArray<Insights::FBaseTreeNodePtr>& GroupChildren = GroupPtr->GetChildren();
		for (const Insights::FBaseTreeNodePtr& Child : GroupChildren)
		{
			const FMemTagNodePtr& NodePtr = StaticCastSharedPtr<FMemTagNode, Insights::FBaseTreeNode>(Child);
			const bool bIsChildVisible = (!bFilterOutZeroCountMemTags || NodePtr->GetAggregatedStats().InstanceCount > 0)
									  && (!Insights::FMemoryTracker::IsValidTrackerId(NodePtr->GetMemTrackerId()) || ((Insights::FMemoryTracker::AsFlag(NodePtr->GetMemTrackerId()) & TrackersFilter) != 0))
									  && Filters->PassesAllFilters(NodePtr);
			if (bIsChildVisible)
			{
				// Add a child node.
				GroupPtr->AddFilteredChild(NodePtr);
				NumVisibleChildren++;
			}
		}

		if (bIsGroupVisible || NumVisibleChildren > 0)
		{
			// Add a group.
			FilteredGroupNodes.Add(GroupPtr);
			GroupPtr->SetExpansion(true);
		}
		else
		{
			GroupPtr->SetExpansion(false);
		}
	}

	// Only expand LLM tag nodes if we have a text filter.
	const bool bNonEmptyTextFilter = !TextFilter->GetRawFilterText().IsEmpty();
	if (bNonEmptyTextFilter)
	{
		if (!bExpansionSaved)
		{
			ExpandedNodes.Empty();
			TreeView->GetExpandedItems(ExpandedNodes);
			bExpansionSaved = true;
		}

		for (int32 Fx = 0; Fx < FilteredGroupNodes.Num(); Fx++)
		{
			const FMemTagNodePtr& GroupPtr = FilteredGroupNodes[Fx];
			TreeView->SetItemExpansion(GroupPtr, GroupPtr->IsExpanded());
		}
	}
	else
	{
		if (bExpansionSaved)
		{
			// Restore previously expanded nodes when the text filter is disabled.
			TreeView->ClearExpandedItems();
			for (auto It = ExpandedNodes.CreateConstIterator(); It; ++It)
			{
				TreeView->SetItemExpansion(*It, true);
			}
			bExpansionSaved = false;
		}
	}

	// Request tree refresh
	TreeView->RequestTreeRefresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::HandleItemToStringArray(const FMemTagNodePtr& FMemTagNodePtr, TArray<FString>& OutSearchStrings) const
{
	OutSearchStrings.Add(FMemTagNodePtr->GetName().GetPlainNameString());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::FilterOutZeroCountMemTags_OnCheckStateChanged(ECheckBoxState NewRadioState)
{
	bFilterOutZeroCountMemTags = (NewRadioState == ECheckBoxState::Checked);
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ECheckBoxState SMemTagTreeView::FilterOutZeroCountMemTags_IsChecked() const
{
	return bFilterOutZeroCountMemTags ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeView
////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_Refresh()
{
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_OnSelectionChanged(FMemTagNodePtr SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		TArray<FMemTagNodePtr> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() == 1 && !SelectedItems[0]->IsGroup())
		{
			//TODO: FMemoryProfilerManager::Get()->SetSelectedMemTag(SelectedItems[0]->GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_OnGetChildren(FMemTagNodePtr InParent, TArray<FMemTagNodePtr>& OutChildren)
{
	const TArray<Insights::FBaseTreeNodePtr>& Children = InParent->GetFilteredChildren();
	OutChildren.Reset(Children.Num());
	for (const Insights::FBaseTreeNodePtr& Child : Children)
	{
		OutChildren.Add(StaticCastSharedPtr<FMemTagNode, Insights::FBaseTreeNode>(Child));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TreeView_OnMouseButtonDoubleClick(FMemTagNodePtr MemTagNodePtr)
{
	if (MemTagNodePtr->IsGroup())
	{
		const bool bIsGroupExpanded = TreeView->IsItemExpanded(MemTagNodePtr);
		TreeView->SetItemExpansion(MemTagNodePtr, !bIsGroupExpanded);
	}
	else
	{
		TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
		if (ProfilerWindow.IsValid())
		{
			FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
			const Insights::FMemoryTrackerId MemTrackerId = MemTagNodePtr->GetMemTrackerId();
			const Insights::FMemoryTagId MemTagId = MemTagNodePtr->GetMemTagId();

			TSharedPtr<FMemoryGraphTrack> GraphTrack = SharedState.GetMemTagGraphTrack(MemTrackerId, MemTagId);
			if (!GraphTrack.IsValid())
			{
				SharedState.CreateMemTagGraphTrack(MemTrackerId, MemTagId);
			}
			else
			{
				SharedState.RemoveMemTagGraphTrack(MemTrackerId, MemTagId);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tree View's Table Row
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> SMemTagTreeView::TreeView_OnGenerateRow(FMemTagNodePtr MemTagNodePtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> TableRow =
		SNew(SMemTagTreeViewTableRow, OwnerTable)
		.OnShouldBeEnabled(this, &SMemTagTreeView::TableRow_ShouldBeEnabled)
		.OnIsColumnVisible(this, &SMemTagTreeView::IsColumnVisible)
		.OnSetHoveredCell(this, &SMemTagTreeView::TableRow_SetHoveredCell)
		.OnGetColumnOutlineHAlignmentDelegate(this, &SMemTagTreeView::TableRow_GetColumnOutlineHAlignment)
		.HighlightText(this, &SMemTagTreeView::TableRow_GetHighlightText)
		.HighlightedNodeName(this, &SMemTagTreeView::TableRow_GetHighlightedNodeName)
		.TablePtr(Table)
		.MemTagNodePtr(MemTagNodePtr);

	return TableRow;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::TableRow_ShouldBeEnabled(FMemTagNodePtr NodePtr) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::TableRow_SetHoveredCell(TSharedPtr<Insights::FTable> InTablePtr, TSharedPtr<Insights::FTableColumn> InColumnPtr, FMemTagNodePtr InNodePtr)
{
	HoveredColumnId = InColumnPtr ? InColumnPtr->GetId() : FName();

	const bool bIsAnyMenusVisible = FSlateApplication::Get().AnyMenusVisible();
	if (!HasMouseCapture() && !bIsAnyMenusVisible)
	{
		HoveredNodePtr = InNodePtr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EHorizontalAlignment SMemTagTreeView::TableRow_GetColumnOutlineHAlignment(const FName ColumnId) const
{
	const TIndirectArray<SHeaderRow::FColumn>& Columns = TreeViewHeaderRow->GetColumns();
	const int32 LastColumnIdx = Columns.Num() - 1;

	// First column
	if (Columns[0].ColumnId == ColumnId)
	{
		return HAlign_Left;
	}
	// Last column
	else if (Columns[LastColumnIdx].ColumnId == ColumnId)
	{
		return HAlign_Right;
	}
	// Middle columns
	{
		return HAlign_Center;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemTagTreeView::TableRow_GetHighlightText() const
{
	return SearchBox->GetText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName SMemTagTreeView::TableRow_GetHighlightedNodeName() const
{
	return HighlightedNodeName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SearchBox
////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SearchBox_OnTextChanged(const FText& InFilterText)
{
	TextFilter->SetRawFilterText(InFilterText);
	SearchBox->SetError(TextFilter->GetFilterErrorText());
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::SearchBox_IsEnabled() const
{
	return MemTagNodes.Num() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Grouping
////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateGroups()
{
	if (GroupingMode == EMemTagNodeGroupingMode::Flat)
	{
		GroupNodes.Reset();

		const FName GroupName(TEXT("All"));
		FMemTagNodePtr GroupPtr = MakeShared<FMemTagNode>(GroupName);
		GroupNodes.Add(GroupPtr);

		for (const FMemTagNodePtr& NodePtr : MemTagNodes)
		{
			GroupPtr->AddChildAndSetParent(NodePtr);
		}
		TreeView->SetItemExpansion(GroupPtr, true);
	}
	// Creates one group for each stat type.
	else if (GroupingMode == EMemTagNodeGroupingMode::ByType)
	{
		TMap<EMemTagNodeType, FMemTagNodePtr> GroupNodeSet;
		for (const FMemTagNodePtr& NodePtr : MemTagNodes)
		{
			const EMemTagNodeType NodeType = NodePtr->GetType();
			FMemTagNodePtr GroupPtr = GroupNodeSet.FindRef(NodeType);
			if (!GroupPtr)
			{
				const FName GroupName = *MemTagNodeTypeHelper::ToText(NodeType).ToString();
				GroupPtr = GroupNodeSet.Add(NodeType, MakeShared<FMemTagNode>(GroupName));
			}
			GroupPtr->AddChildAndSetParent(NodePtr);
			TreeView->SetItemExpansion(GroupPtr, true);
		}
		GroupNodeSet.KeySort([](const EMemTagNodeType& A, const EMemTagNodeType& B) { return A < B; }); // sort groups by type
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Creates one group for one letter.
	else if (GroupingMode == EMemTagNodeGroupingMode::ByName)
	{
		TMap<TCHAR, FMemTagNodePtr> GroupNodeSet;
		for (const FMemTagNodePtr& NodePtr : MemTagNodes)
		{
			FString FirstLetterStr(NodePtr->GetName().GetPlainNameString().Left(1).ToUpper());
			const TCHAR FirstLetter = FirstLetterStr[0];
			FMemTagNodePtr GroupPtr = GroupNodeSet.FindRef(FirstLetter);
			if (!GroupPtr)
			{
				const FName GroupName(FirstLetterStr);
				GroupPtr = GroupNodeSet.Add(FirstLetter, MakeShared<FMemTagNode>(GroupName));
			}
			GroupPtr->AddChildAndSetParent(NodePtr);
		}
		GroupNodeSet.KeySort([](const TCHAR& A, const TCHAR& B) { return A < B; }); // sort groups alphabetically
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Groups LLM tags by tracker.
	else if (GroupingMode == EMemTagNodeGroupingMode::ByTracker)
	{
		TMap<Insights::FMemoryTrackerId, FMemTagNodePtr> GroupNodeSet;
		for (const FMemTagNodePtr& NodePtr : MemTagNodes)
		{
			Insights::FMemoryTrackerId TrackerId = NodePtr->GetMemTrackerId();
			FMemTagNodePtr GroupPtr = GroupNodeSet.FindRef(TrackerId);
			if (!GroupPtr)
			{
				const FName GroupName = *NodePtr->GetTrackerText().ToString();
				GroupPtr = GroupNodeSet.Add(TrackerId, MakeShared<FMemTagNode>(GroupName));
			}
			GroupPtr->AddChildAndSetParent(NodePtr);
			TreeView->SetItemExpansion(GroupPtr, true);
		}
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Groups LLM tags by their hierarchy.
	else if (GroupingMode == EMemTagNodeGroupingMode::ByParent)
	{
		TMap<FName, FMemTagNodePtr> GroupNodeSet;
		for (const FMemTagNodePtr& NodePtr : MemTagNodes)
		{
			const FName GroupName = NodePtr->GetParentTagNode() ? NodePtr->GetParentTagNode()->GetName() : FName(TEXT("<LLM>"));
			FMemTagNodePtr GroupPtr = GroupNodeSet.FindRef(GroupName);
			if (!GroupPtr)
			{
				GroupPtr = GroupNodeSet.Add(GroupName, MakeShared<FMemTagNode>(GroupName));
			}
			GroupPtr->AddChildAndSetParent(NodePtr);
			TreeView->SetItemExpansion(GroupPtr, true);
		}
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateGroupByOptionsSources()
{
	GroupByOptionsSource.Reset(4);

	// Must be added in order of elements in the EMemTagNodeGroupingMode.
	GroupByOptionsSource.Add(MakeShared<EMemTagNodeGroupingMode>(EMemTagNodeGroupingMode::Flat));
	GroupByOptionsSource.Add(MakeShared<EMemTagNodeGroupingMode>(EMemTagNodeGroupingMode::ByName));
	//GroupByOptionsSource.Add(MakeShared<EMemTagNodeGroupingMode>(EMemTagNodeGroupingMode::ByType));
	GroupByOptionsSource.Add(MakeShared<EMemTagNodeGroupingMode>(EMemTagNodeGroupingMode::ByTracker));
	GroupByOptionsSource.Add(MakeShared<EMemTagNodeGroupingMode>(EMemTagNodeGroupingMode::ByParent));

	EMemTagNodeGroupingModePtr* GroupingModePtrPtr = GroupByOptionsSource.FindByPredicate([&](const EMemTagNodeGroupingModePtr InGroupingModePtr) { return *InGroupingModePtr == GroupingMode; });
	if (GroupingModePtrPtr != nullptr)
	{
		GroupByComboBox->SetSelectedItem(*GroupingModePtrPtr);
	}

	GroupByComboBox->RefreshOptions();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::GroupBy_OnSelectionChanged(TSharedPtr<EMemTagNodeGroupingMode> NewGroupingMode, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		GroupingMode = *NewGroupingMode;

		CreateGroups();
		SortTreeNodes();
		ApplyFiltering();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeView::GroupBy_OnGenerateWidget(TSharedPtr<EMemTagNodeGroupingMode> InGroupingMode) const
{
	return SNew(STextBlock)
		.Text(MemTagNodeGroupingHelper::ToText(*InGroupingMode))
		.ToolTipText(MemTagNodeGroupingHelper::ToDescription(*InGroupingMode));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemTagTreeView::GroupBy_GetSelectedText() const
{
	return MemTagNodeGroupingHelper::ToText(GroupingMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemTagTreeView::GroupBy_GetSelectedTooltipText() const
{
	return MemTagNodeGroupingHelper::ToDescription(GroupingMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorting
////////////////////////////////////////////////////////////////////////////////////////////////////

const FName SMemTagTreeView::GetDefaultColumnBeingSorted()
{
	return FMemTagTreeViewColumns::TrackerColumnID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const EColumnSortMode::Type SMemTagTreeView::GetDefaultColumnSortMode()
{
	return EColumnSortMode::Ascending;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateSortings()
{
	AvailableSorters.Reset();
	CurrentSorter = nullptr;

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		if (ColumnRef->CanBeSorted())
		{
			TSharedPtr<Insights::ITableCellValueSorter> SorterPtr = ColumnRef->GetValueSorter();
			if (ensure(SorterPtr.IsValid()))
			{
				AvailableSorters.Add(SorterPtr);
			}
		}
	}

	UpdateCurrentSortingByColumn();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::UpdateCurrentSortingByColumn()
{
	TSharedPtr<Insights::FTableColumn> ColumnPtr = Table->FindColumn(ColumnBeingSorted);
	CurrentSorter = ColumnPtr.IsValid() ? ColumnPtr->GetValueSorter() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SortTreeNodes()
{
	if (CurrentSorter.IsValid())
	{
		// Sort groups (always by name).
		TArray<Insights::FBaseTreeNodePtr> SortedGroupNodes;
		for (const FMemTagNodePtr& NodePtr : GroupNodes)
		{
			SortedGroupNodes.Add(NodePtr);
		}
		TSharedPtr<Insights::ITableCellValueSorter> Sorter = CurrentSorter;
		Insights::ESortMode SortMode = (ColumnSortMode == EColumnSortMode::Type::Descending) ? Insights::ESortMode::Descending : Insights::ESortMode::Ascending;
		if (CurrentSorter->GetName() != FName(TEXT("ByName")))
		{
			Sorter = MakeShared<Insights::FSorterByName>(Table->GetColumns()[0]);
			SortMode = Insights::ESortMode::Ascending;
		}
		Sorter->Sort(SortedGroupNodes, SortMode);
		GroupNodes.Reset();
		for (const Insights::FBaseTreeNodePtr& NodePtr : SortedGroupNodes)
		{
			GroupNodes.Add(StaticCastSharedPtr<FMemTagNode>(NodePtr));
		}

		// Sort nodes in each group.
		for (FMemTagNodePtr& Root : GroupNodes)
		{
			SortTreeNodesRec(*Root, *CurrentSorter);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SortTreeNodesRec(FMemTagNode& Node, const Insights::ITableCellValueSorter& Sorter)
{
	Insights::ESortMode SortMode = (ColumnSortMode == EColumnSortMode::Type::Descending) ? Insights::ESortMode::Descending : Insights::ESortMode::Ascending;
	Node.SortChildren(Sorter, SortMode);

	for (Insights::FBaseTreeNodePtr ChildPtr : Node.GetChildren())
	{
		//if (ChildPtr->IsGroup())
		if (ChildPtr->GetChildrenCount() > 0)
		{
			SortTreeNodesRec(*StaticCastSharedPtr<FMemTagNode>(ChildPtr), Sorter);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EColumnSortMode::Type SMemTagTreeView::GetSortModeForColumn(const FName ColumnId) const
{
	if (ColumnBeingSorted != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return ColumnSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SetSortModeForColumn(const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	ColumnBeingSorted = ColumnId;
	ColumnSortMode = SortMode;
	UpdateCurrentSortingByColumn();

	SortTreeNodes();
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	SetSortModeForColumn(ColumnId, SortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (HeaderMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::HeaderMenu_SortMode_IsChecked(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	return ColumnBeingSorted == ColumnId && ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::HeaderMenu_SortMode_CanExecute(const FName ColumnId, const EColumnSortMode::Type InSortMode) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeSorted();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::HeaderMenu_SortMode_Execute(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnId, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_SortMode_IsChecked(const EColumnSortMode::Type InSortMode)
{
	return ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_SortMode_CanExecute(const EColumnSortMode::Type InSortMode) const
{
	return true; //ColumnSortMode != InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ContextMenu_SortMode_Execute(const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnBeingSorted, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortByColumn action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_SortByColumn_IsChecked(const FName ColumnId)
{
	return ColumnId == ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_SortByColumn_CanExecute(const FName ColumnId) const
{
	return true; //ColumnId != ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ContextMenu_SortByColumn_Execute(const FName ColumnId)
{
	SetSortModeForColumn(ColumnId, EColumnSortMode::Descending);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanShowColumn(const FName ColumnId) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ShowColumn(const FName ColumnId)
{
	Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	Column.Show();

	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(Column.GetId())
		.DefaultLabel(Column.GetShortName())
		.ToolTip(SMemTagTreeViewTooltip::GetColumnTooltip(Column))
		.HAlignHeader(Column.GetHorizontalAlignment())
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill)
		.InitialSortMode(Column.GetInitialSortMode())
		.SortMode(this, &SMemTagTreeView::GetSortModeForColumn, Column.GetId())
		.OnSort(this, &SMemTagTreeView::OnSortModeChanged)
		.FillWidth(Column.GetInitialWidth())
		//.FixedWidth(Column.IsFixedWidth() ? Column.GetInitialWidth() : TOptional<float>())
		.HeaderContent()
		[
			SNew(SBox)
			.HeightOverride(24.0f)
			.Padding(FMargin(0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SMemTagTreeView::GetColumnHeaderText, Column.GetId())
			]
		]
		.MenuContent()
		[
			TreeViewHeaderRow_GenerateColumnMenu(Column)
		];

	int32 ColumnIndex = 0;
	const int32 NewColumnPosition = Table->GetColumnPositionIndex(ColumnId);
	const int32 NumColumns = TreeViewHeaderRow->GetColumns().Num();
	for (; ColumnIndex < NumColumns; ColumnIndex++)
	{
		const SHeaderRow::FColumn& CurrentColumn = TreeViewHeaderRow->GetColumns()[ColumnIndex];
		const int32 CurrentColumnPosition = Table->GetColumnPositionIndex(CurrentColumn.ColumnId);
		if (NewColumnPosition < CurrentColumnPosition)
		{
			break;
		}
	}

	TreeViewHeaderRow->InsertColumn(ColumnArgs, ColumnIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// HideColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanHideColumn(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::HideColumn(const FName ColumnId)
{
	Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	Column.Hide();

	TreeViewHeaderRow->RemoveColumn(ColumnId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ToggleColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::IsColumnVisible(const FName ColumnId)
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanToggleColumnVisibility(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return !Column.IsVisible() || Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ToggleColumnVisibility(const FName ColumnId)
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	if (Column.IsVisible())
	{
		HideColumn(ColumnId);
	}
	else
	{
		ShowColumn(ColumnId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// "Show All Columns" action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_ShowAllColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ContextMenu_ShowAllColumns_Execute()
{
	ColumnBeingSorted = GetDefaultColumnBeingSorted();
	ColumnSortMode = GetDefaultColumnSortMode();
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (!Column.IsVisible())
		{
			ShowColumn(Column.GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// "Show Min/Max/Median Columns" action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_ShowMinMaxMedColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ContextMenu_ShowMinMaxMedColumns_Execute()
{
	TSet<FName> Preset =
	{
		FMemTagTreeViewColumns::NameColumnID,
		//FMemTagTreeViewColumns::MetaGroupNameColumnID,
		//FMemTagTreeViewColumns::TypeColumnID,
		FMemTagTreeViewColumns::TrackerColumnID,
		FMemTagTreeViewColumns::InstanceCountColumnID,

		FMemTagTreeViewColumns::MaxValueColumnID,
		FMemTagTreeViewColumns::AverageValueColumnID,
		//FMemTagTreeViewColumns::MedianValueColumnID,
		FMemTagTreeViewColumns::MinValueColumnID,
	};

	ColumnBeingSorted = FMemTagTreeViewColumns::MaxValueColumnID;
	ColumnSortMode = EColumnSortMode::Descending;
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		const bool bShouldBeVisible = Preset.Contains(Column.GetId());

		if (bShouldBeVisible && !Column.IsVisible())
		{
			ShowColumn(Column.GetId());
		}
		else if (!bShouldBeVisible && Column.IsVisible())
		{
			HideColumn(Column.GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ResetColumns action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::ContextMenu_ResetColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ContextMenu_ResetColumns_Execute()
{
	ColumnBeingSorted = GetDefaultColumnBeingSorted();
	ColumnSortMode = GetDefaultColumnSortMode();
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (Column.ShouldBeVisible() && !Column.IsVisible())
		{
			ShowColumn(Column.GetId());
		}
		else if (!Column.ShouldBeVisible() && Column.IsVisible())
		{
			HideColumn(Column.GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::Reset()
{
	StatsStartTime = 0.0;
	StatsEndTime = 0.0;

	RebuildTree(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// We need to check if the list of LLM tags has changed.
	// But, ensure we do not check too often.
	static uint64 NextTimestamp = 0;
	uint64 Time = FPlatformTime::Cycles64();
	if (Time > NextTimestamp)
	{
		RebuildTree(false);

		// 1000 counters --> check each 150ms
		// 10000 counters --> check each 600ms
		// 100000 counters --> check each 5.1s
		const double WaitTimeSec = 0.1 + MemTagNodes.Num() / 20000.0;
		const uint64 WaitTime = static_cast<uint64>(WaitTimeSec / FPlatformTime::GetSecondsPerCycle64());
		NextTimestamp = Time + WaitTime;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::RebuildTree(bool bResync)
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	FStopwatch SyncStopwatch;
	SyncStopwatch.Start();

	bool bListHasChanged = false;

	if (bResync)
	{
		LastMemoryTagListSerialNumber = 0;
		MemTagNodes.Empty();
		MemTagNodesIdMap.Empty();
		bListHasChanged = true;
	}

	const uint32 PreviousNodeCount = MemTagNodes.Num();

	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		const Insights::FMemoryTagList& TagList = SharedState.GetTagList();

		if (LastMemoryTagListSerialNumber != TagList.GetSerialNumber())
		{
			LastMemoryTagListSerialNumber = TagList.GetSerialNumber();

			const TArray<Insights::FMemoryTag*>& MemTags = TagList.GetTags();
			const int32 MemTagCount = MemTags.Num();

			MemTagNodes.Empty(MemTagCount);
			MemTagNodesIdMap.Empty(MemTagCount);
			bListHasChanged = true;

			for (Insights::FMemoryTag* MemTagPtr : MemTags)
			{
				FMemTagNodePtr MemTagNodePtr = MakeShared<FMemTagNode>(MemTagPtr);
				MemTagNodes.Add(MemTagNodePtr);
				MemTagNodesIdMap.Add(MemTagPtr->GetId(), MemTagNodePtr);
			}

			// Resolve pointers to parent tags.
			for (FMemTagNodePtr& NodePtr : MemTagNodes)
			{
				check(NodePtr->GetMemTag() != nullptr);
				Insights::FMemoryTag& MemTag = *NodePtr->GetMemTag();

				FMemTagNodePtr ParentNodePtr = MemTagNodesIdMap.FindRef(MemTag.GetParentId());
				if (ParentNodePtr)
				{
					check(ParentNodePtr != NodePtr);
					NodePtr->SetParentTagNode(ParentNodePtr);
				}
			}
		}
	}

	SyncStopwatch.Stop();

	if (bListHasChanged)
	{
		UpdateTree();
		UpdateStatsInternal();

		// Save selection.
		TArray<FMemTagNodePtr> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		TreeView->RebuildList();

		// Restore selection.
		if (SelectedItems.Num() > 0)
		{
			TreeView->ClearSelection();
			for (FMemTagNodePtr& NodePtr : SelectedItems)
			{
				NodePtr = GetMemTagNode(NodePtr->GetMemTagId());
			}
			SelectedItems.RemoveAll([](const FMemTagNodePtr& NodePtr) { return !NodePtr.IsValid(); });
			if (SelectedItems.Num() > 0)
			{
				TreeView->SetItemSelection(SelectedItems, true);
				TreeView->RequestScrollIntoView(SelectedItems.Last());
			}
		}
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.01)
	{
		const double SyncTime = SyncStopwatch.GetAccumulatedTime();
		UE_LOG(MemoryProfiler, Log, TEXT("[LLM Tags] Tree view rebuilt in %.4fs (sync: %.4fs + update: %.4fs) --> %d LLM tags (%d added)"),
			TotalTime, SyncTime, TotalTime - SyncTime, MemTagNodes.Num(), MemTagNodes.Num() - PreviousNodeCount);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ResetStats()
{
	StatsStartTime = 0.0;
	StatsEndTime = 0.0;

	UpdateStatsInternal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::UpdateStats(double StartTime, double EndTime)
{
	StatsStartTime = StartTime;
	StatsEndTime = EndTime;

	UpdateStatsInternal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::UpdateStatsInternal()
{
	if (StatsStartTime >= StatsEndTime)
	{
		// keep previous aggregated stats
		return;
	}

	FStopwatch AggregationStopwatch;
	FStopwatch Stopwatch;
	Stopwatch.Start();

	for (const FMemTagNodePtr& NodePtr : MemTagNodes)
	{
		NodePtr->ResetAggregatedStats();
	}

	/*
	if (Session.IsValid())
	{
		TUniquePtr<TraceServices::ITable<TraceServices::FMemoryProfilerAggregatedStats>> AggregationResultTable;

		AggregationStopwatch.Start();
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::IMemoryProfilerProvider& MemoryProfilerProvider = TraceServices::ReadMemoryProfilerProvider(*Session.Get());
			AggregationResultTable.Reset(MemoryProfilerProvider.CreateAggregation(StatsStartTime, StatsEndTime));
		}
		AggregationStopwatch.Stop();

		if (AggregationResultTable.IsValid())
		{
			TUniquePtr<TraceServices::ITableReader<TraceServices::FMemoryProfilerAggregatedStats>> TableReader(AggregationResultTable->CreateReader());
			while (TableReader->IsValid())
			{
				const TraceServices::FMemoryProfilerAggregatedStats* Row = TableReader->GetCurrentRow();
				FMemTagNodePtr* MemTagNodePtrPtr = MemTagNodesIdMap.Find(static_cast<uint64>(Row->EventTypeIndex));
				if (MemTagNodePtrPtr != nullptr)
				{
					FMemTagNodePtr MemTagNodePtr = *MemTagNodePtrPtr;
					->SetAggregatedStats(*Row);

					TSharedPtr<ITableRow> TableRowPtr = TreeView->WidgetFromItem(MemTagNodePtr);
					if (TableRowPtr.IsValid())
					{
						TSharedPtr<SMemTagsTableRow> RowPtr = StaticCastSharedPtr<SMemTagsTableRow, ITableRow>(TableRowPtr);
						RowPtr->InvalidateContent();
					}
				}
				TableReader->NextRow();
			}
		}
	}
	*/

	UpdateTree();

	const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	if (SelectedNodes.Num() > 0)
	{
		TreeView->RequestScrollIntoView(SelectedNodes[0]);
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	const double AggregationTime = AggregationStopwatch.GetAccumulatedTime();
	UE_LOG(MemoryProfiler, Log, TEXT("[LLM Tags] Aggregated stats updated in %.4fs (%.4fs + %.4fs)"),
		TotalTime, AggregationTime, TotalTime - AggregationTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SelectMemTagNode(Insights::FMemoryTagId MemTagId)
{
	FMemTagNodePtr NodePtr = GetMemTagNode(MemTagId);
	if (NodePtr)
	{
		TreeView->SetSelection(NodePtr);
		TreeView->RequestScrollIntoView(NodePtr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Load Report XML button action
////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemTagTreeView::LoadReportXML_OnClicked()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		TArray<FString> OutFiles;
		bool bOpened = false;

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform != nullptr)
		{
			FSlateApplication::Get().CloseToolTip();

			const FString DefaultPath(FPaths::RootDir() / TEXT("Engine/Binaries/DotNET/CsvTools"));
			const FString DefaultFile(TEXT("LLMReportTypes.xml"));

			bOpened = DesktopPlatform->OpenFileDialog
			(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				LOCTEXT("LoadReportXML_FileDesc", "Open the LLMReportTypes.xml file...").ToString(),
				DefaultPath,
				DefaultFile, // Not actually used. See FDesktopPlatformWindows::FileDialogShared implementation. :(
				LOCTEXT("LoadReportXML_FileFilter", "XML files (*.xml)|*.xml|All files (*.*)|*.*").ToString(),
				EFileDialogFlags::None,
				OutFiles
			);
		}

		if (bOpened == true && OutFiles.Num() == 1)
		{
			FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
			SharedState.RemoveAllMemTagGraphTracks();
			SharedState.CreateTracksFromReport(OutFiles[0]);
		}
	}

	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Button actions re graph tracks
////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemTagTreeView::ShowAllTracks_OnClicked()
{
	CreateGraphTracksForFilteredMemTags();
	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemTagTreeView::HideAllTracks_OnClicked()
{
	RemoveAllGraphTracks();
	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Create memory graph tracks for selected LLM tag(s)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanCreateGraphTracksForSelectedMemTags() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	return ProfilerWindow.IsValid() && TreeView->GetNumItemsSelected() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateGraphTracksForSelectedMemTags()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
		for (const FMemTagNodePtr& SelectedMemTagNode : SelectedNodes)
		{
			if (SelectedMemTagNode->IsGroup())
			{
				const TArray<Insights::FBaseTreeNodePtr>& Children = SelectedMemTagNode->GetFilteredChildren();
				for (const Insights::FBaseTreeNodePtr& Child : Children)
				{
					FMemTagNodePtr MemTagNode = StaticCastSharedPtr<FMemTagNode, Insights::FBaseTreeNode>(Child);
					const Insights::FMemoryTrackerId MemTrackerId = MemTagNode->GetMemTrackerId();
					const Insights::FMemoryTagId MemTagId = MemTagNode->GetMemTagId();
					SharedState.CreateMemTagGraphTrack(MemTrackerId, MemTagId);
				}
			}
			else
			{
				const Insights::FMemoryTrackerId MemTrackerId = SelectedMemTagNode->GetMemTrackerId();
				const Insights::FMemoryTagId MemTagId = SelectedMemTagNode->GetMemTagId();
				SharedState.CreateMemTagGraphTrack(MemTrackerId, MemTagId);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Create memory graph tracks for filtered LLM tags
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanCreateGraphTracksForFilteredMemTags() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (!ProfilerWindow.IsValid())
	{
		return false;
	}
	int32 FilteredNodeCount = 0;
	for (const FMemTagNodePtr& Group : FilteredGroupNodes)
	{
		FilteredNodeCount += Group->GetFilteredChildren().Num();
	}
	return FilteredNodeCount > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateGraphTracksForFilteredMemTags()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		for (const FMemTagNodePtr& GroupNode : FilteredGroupNodes)
		{
			const TArray<Insights::FBaseTreeNodePtr>& Children = GroupNode->GetFilteredChildren();
			for (const Insights::FBaseTreeNodePtr& Child : Children)
			{
				FMemTagNodePtr MemTagNode = StaticCastSharedPtr<FMemTagNode, Insights::FBaseTreeNode>(Child);
				const Insights::FMemoryTrackerId MemTrackerId = MemTagNode->GetMemTrackerId();
				const Insights::FMemoryTagId MemTagId = MemTagNode->GetMemTagId();
				SharedState.CreateMemTagGraphTrack(MemTrackerId, MemTagId);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Create all mem memory graph tracks for selected LLM tag(s)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanCreateAllGraphTracks() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	return ProfilerWindow.IsValid();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::CreateAllGraphTracks()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		for (const FMemTagNodePtr& MemTagNode : MemTagNodes)
		{
			const Insights::FMemoryTrackerId MemTrackerId = MemTagNode->GetMemTrackerId();
			const Insights::FMemoryTagId MemTagId = MemTagNode->GetMemTagId();
			SharedState.CreateMemTagGraphTrack(MemTrackerId, MemTagId);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Remove memory graph tracks for selected LLM tag(s)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanRemoveGraphTracksForSelectedMemTags() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	return ProfilerWindow.IsValid() && TreeView->GetNumItemsSelected() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::RemoveGraphTracksForSelectedMemTags()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
		for (const FMemTagNodePtr& SelectedMemTagNode : SelectedNodes)
		{
			if (SelectedMemTagNode->IsGroup())
			{
				const TArray<Insights::FBaseTreeNodePtr>& Children = SelectedMemTagNode->GetFilteredChildren();
				for (const Insights::FBaseTreeNodePtr& Child : Children)
				{
					FMemTagNodePtr MemTagNode = StaticCastSharedPtr<FMemTagNode, Insights::FBaseTreeNode>(Child);
					const Insights::FMemoryTrackerId MemTrackerId = MemTagNode->GetMemTrackerId();
					const Insights::FMemoryTagId MemTagId = MemTagNode->GetMemTagId();
					SharedState.RemoveMemTagGraphTrack(MemTrackerId, MemTagId);
				}
			}
			else
			{
				const Insights::FMemoryTrackerId MemTrackerId = SelectedMemTagNode->GetMemTrackerId();
				const Insights::FMemoryTagId MemTagId = SelectedMemTagNode->GetMemTagId();
				SharedState.RemoveMemTagGraphTrack(MemTrackerId, MemTagId);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Remove all graph series
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanRemoveAllGraphTracks() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	return ProfilerWindow.IsValid();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::RemoveAllGraphTracks()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		SharedState.RemoveAllMemTagGraphTracks();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Generate new color for selected LLM tag(s)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanGenerateColorForSelectedMemTags() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	return ProfilerWindow.IsValid() && TreeView->GetNumItemsSelected() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::GenerateColorForSelectedMemTags()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
		for (const FMemTagNodePtr& SelectedMemTagNode : SelectedNodes)
		{
			constexpr bool bSetRandomColor = true;
			SetColorToNode(SelectedMemTagNode, FLinearColor(), bSetRandomColor);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SetColorToNode(const FMemTagNodePtr& MemTagNode, FLinearColor Color, bool bSetRandomColor)
{
	if (MemTagNode->IsGroup())
	{
		const TArray<Insights::FBaseTreeNodePtr>& Children = MemTagNode->GetFilteredChildren();
		for (const Insights::FBaseTreeNodePtr& Child : Children)
		{
			const FMemTagNodePtr ChildMemTagNode = StaticCastSharedPtr<FMemTagNode, Insights::FBaseTreeNode>(Child);
			SetColorToNode(ChildMemTagNode, Color, bSetRandomColor);
		}
		return;
	}

	Insights::FMemoryTag* MemTag = MemTagNode->GetMemTag();
	if (!MemTag)
	{
		return;
	}

	if (bSetRandomColor)
	{
		MemTag->SetRandomColor();
		Color = MemTag->GetColor();
	}
	else
	{
		MemTag->SetColor(Color);
	}

	const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);

	const Insights::FMemoryTagId MemTagId = MemTagNode->GetMemTagId();

	TSharedPtr<FMemoryGraphTrack> MainGraphTrack;
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		MainGraphTrack = SharedState.GetMainGraphTrack();
	}

	for (const TSharedPtr<FMemoryGraphTrack>& GraphTrack : MemTag->GetGraphTracks())
	{
		for (TSharedPtr<FGraphSeries>& Series : GraphTrack->GetSeries())
		{
			//TODO: if (Series->Is<FMemoryGraphSeries>())
			TSharedPtr<FMemoryGraphSeries> MemorySeries = StaticCastSharedPtr<FMemoryGraphSeries>(Series);
			if (MemorySeries->GetTagId() == MemTagId)
			{
				if (GraphTrack == MainGraphTrack)
				{
					MemorySeries->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				}
				else
				{
					MemorySeries->SetColor(Color, BorderColor);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Edit color for selected LLM tag(s)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeView::CanEditColorForSelectedMemTags() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	return ProfilerWindow.IsValid() && TreeView->GetNumItemsSelected() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::EditColorForSelectedMemTags()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		EditableColorValue = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
		const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
		if (SelectedNodes.Num() > 0)
		{
			EditableColorValue = SelectedNodes[0]->GetColor();
		}

		FColorPickerArgs PickerArgs;
		{
			PickerArgs.bUseAlpha = true;
			PickerArgs.bOnlyRefreshOnMouseUp = false;
			PickerArgs.bOnlyRefreshOnOk = false;
			PickerArgs.bExpandAdvancedSection = false;
			//PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
			PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SMemTagTreeView::SetEditableColor);
			PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &SMemTagTreeView::ColorPickerCancelled);
			//PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &SMemTagTreeView::InteractivePickBegin);
			//PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &SMemTagTreeView::InteractivePickEnd);
			//PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &SMemTagTreeView::ColorPickerClosed);
			PickerArgs.InitialColor = EditableColorValue;
			PickerArgs.ParentWidget = SharedThis(this);
		}

		OpenColorPicker(PickerArgs);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FLinearColor SMemTagTreeView::GetEditableColor() const
{
	return EditableColorValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::SetEditableColor(FLinearColor NewColor)
{
	EditableColorValue = NewColor;

	const TArray<FMemTagNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	for (const FMemTagNodePtr& SelectedMemTagNode : SelectedNodes)
	{
		constexpr bool bSetRandomColor = false;
		SetColorToNode(SelectedMemTagNode, EditableColorValue, bSetRandomColor);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeView::ColorPickerCancelled(FLinearColor OriginalColor)
{
	SetEditableColor(OriginalColor);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
