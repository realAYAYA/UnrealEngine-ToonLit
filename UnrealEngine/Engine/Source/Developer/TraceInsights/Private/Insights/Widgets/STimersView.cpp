// Copyright Epic Games, Inc. All Rights Reserved.

#include "STimersView.h"

#include "DesktopPlatformModule.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFileManager.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"
#include "Logging/MessageLog.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Views/STableViewBase.h"

// Insights
#include "Insights/Common/Stopwatch.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsStyle.h"
#include "Insights/Log.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/TimingProfilerCommon.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/ThreadTimingTrack.h"
#include "Insights/ViewModels/TimerAggregation.h"
#include "Insights/ViewModels/TimerNodeHelper.h"
#include "Insights/ViewModels/TimersViewColumnFactory.h"
#include "Insights/ViewModels/TimingExporter.h"
#include "Insights/ViewModels/TimingGraphTrack.h"
#include "Insights/Widgets/SAsyncOperationStatus.h"
#include "Insights/Widgets/STimersViewTooltip.h"
#include "Insights/Widgets/STimerTableRow.h"
#include "Insights/Widgets/STimingProfilerWindow.h"
#include "Insights/Widgets/STimingView.h"

#include <limits>

#define LOCTEXT_NAMESPACE "STimersView"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimersViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimersViewCommands : public TCommands<FTimersViewCommands>
{
public:
	FTimersViewCommands()
	: TCommands<FTimersViewCommands>(
		TEXT("TimersViewCommands"),
		NSLOCTEXT("Contexts", "TimersViewCommands", "Insights - Timers View"),
		NAME_None,
		FInsightsStyle::Get().GetStyleSetName())
	{
	}

	virtual ~FTimersViewCommands()
	{
	}

	// UI_COMMAND takes long for the compiler to optimize
	PRAGMA_DISABLE_OPTIMIZATION
	virtual void RegisterCommands() override
	{
		UI_COMMAND(Command_CopyToClipboard, "Copy To Clipboard", "Copies the selection (timers and their aggregated statistics) to clipboard.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
		UI_COMMAND(Command_Export, "Export...", "Exports the selection (timers and their aggregated statistics) to a text file (tab-separated values or comma-separated values).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));
		UI_COMMAND(Command_ExportTimingEventsSelection, "Export Timing Events (Selection)...", "Exports the timing events to a text file (tab-separated values or comma-separated values).\nOnly exports the timing events of the selected timer(s), for the visible CPU/GPU Thread tracks and in the selected time region (if any).", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Command_ExportTimingEvents, "Export Timing Events (All)...", "Exports all the timing events (for all CPU/GPU threads) to a text file (tab-separated values or comma-separated values).", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Command_ExportThreads, "Export Threads...", "Exports the list of threads to a text file (tab-separated values or comma-separated values).", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Command_ExportTimers, "Export Timers...", "Exports the list of timers to a text file (tab-separated values or comma-separated values).", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Command_OpenSource, "Open Source", "Opens the source file of the selected timer in the registered IDE.", EUserInterfaceActionType::Button, FInputChord());
	}
	PRAGMA_ENABLE_OPTIMIZATION

	TSharedPtr<FUICommandInfo> Command_CopyToClipboard;
	TSharedPtr<FUICommandInfo> Command_Export;
	TSharedPtr<FUICommandInfo> Command_ExportTimingEventsSelection;
	TSharedPtr<FUICommandInfo> Command_ExportTimingEvents;
	TSharedPtr<FUICommandInfo> Command_ExportThreads;
	TSharedPtr<FUICommandInfo> Command_ExportTimers;
	TSharedPtr<FUICommandInfo> Command_OpenSource;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// STimersView
////////////////////////////////////////////////////////////////////////////////////////////////////

STimersView::STimersView()
	: Table(MakeShared<Insights::FTable>())
	, bExpansionSaved(false)
	, bFilterOutZeroCountTimers(false)
	, GroupingMode(ETimerGroupingMode::ByType)
	, AvailableSorters()
	, CurrentSorter(nullptr)
	, ColumnBeingSorted(GetDefaultColumnBeingSorted())
	, ColumnSortMode(GetDefaultColumnSortMode())
	, Aggregator(MakeShared<Insights::FTimerAggregator>())
{
	FMemory::Memset(bTimerTypeIsVisible, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

STimersView::~STimersView()
{
	// Remove ourselves from the Insights manager.
	if (FInsightsManager::Get().IsValid())
	{
		FInsightsManager::Get()->GetSessionChangedEvent().RemoveAll(this);
		FInsightsManager::Get()->GetSessionAnalysisCompletedEvent().RemoveAll(this);
	}

	FTimersViewCommands::Unregister();

	Session.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::InitCommandList()
{
	FTimersViewCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(FTimersViewCommands::Get().Command_CopyToClipboard, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_CopyToClipboard_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_CopyToClipboard_CanExecute));
	CommandList->MapAction(FTimersViewCommands::Get().Command_Export, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_Export_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_Export_CanExecute));
	CommandList->MapAction(FTimersViewCommands::Get().Command_ExportTimingEventsSelection, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportTimingEventsSelection_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportTimingEventsSelection_CanExecute));
	CommandList->MapAction(FTimersViewCommands::Get().Command_ExportTimingEvents, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportTimingEvents_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportTimingEvents_CanExecute));
	CommandList->MapAction(FTimersViewCommands::Get().Command_ExportThreads, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportThreads_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportThreads_CanExecute));
	CommandList->MapAction(FTimersViewCommands::Get().Command_ExportTimers, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportTimers_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ExportTimers_CanExecute));
	CommandList->MapAction(FTimersViewCommands::Get().Command_OpenSource, FExecuteAction::CreateSP(this, &STimersView::ContextMenu_OpenSource_Execute), FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_OpenSource_CanExecute));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STimersView::Construct(const FArguments& InArgs)
{
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

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Search box
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchBoxHint", "Search timers or groups"))
					.OnTextChanged(this, &STimersView::SearchBox_OnTextChanged)
					.IsEnabled(this, &STimersView::SearchBox_IsEnabled)
					.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search timer or group."))
				]

				// Filter by type (Gpu)
				+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 4.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					GetToggleButtonForTimerType(ETimerNodeType::GpuScope)
				]

				// Filter by type (Compute)
				//+ SHorizontalBox::Slot()
				//.Padding(4.0f, 0.0f, 4.0f, 0.0f)
				//.AutoWidth()
				//.VAlign(VAlign_Center)
				//[
				//	GetToggleButtonForTimerType(ETimerNodeType::ComputeScope)
				//]

				// Filter by type (Cpu)
				+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 4.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					GetToggleButtonForTimerType(ETimerNodeType::CpuScope)
				]

				// Filter out timers with zero instance count
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.HAlign(HAlign_Center)
					.Padding(3.0f)
					.OnCheckStateChanged(this, &STimersView::FilterOutZeroCountTimers_OnCheckStateChanged)
					.IsChecked(this, &STimersView::FilterOutZeroCountTimers_IsChecked)
					.ToolTipText(LOCTEXT("FilterOutZeroCountTimers_Tooltip", "Filter out the timers having zero total instance count (aggregated stats)."))
					[
						SNew(SImage)
						.Image(FInsightsStyle::Get().GetBrush("Icons.ZeroCountFilter"))
					]
				]
			]

			// Group by
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GroupByText", "Group by"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(128.0f)
					[
						SAssignNew(GroupByComboBox, SComboBox<TSharedPtr<ETimerGroupingMode>>)
						.ToolTipText(this, &STimersView::GroupBy_GetSelectedTooltipText)
						.OptionsSource(&GroupByOptionsSource)
						.OnSelectionChanged(this, &STimersView::GroupBy_OnSelectionChanged)
						.OnGenerateWidget(this, &STimersView::GroupBy_OnGenerateWidget)
						[
							SNew(STextBlock)
							.Text(this, &STimersView::GroupBy_GetSelectedText)
						]
					]
				]
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
				SNew(SOverlay)

				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(TreeView, STreeView<FTimerNodePtr>)
					.ExternalScrollbar(ExternalScrollbar)
					.SelectionMode(ESelectionMode::Multi)
					.TreeItemsSource(&FilteredGroupNodes)
					.OnGetChildren(this, &STimersView::TreeView_OnGetChildren)
					.OnGenerateRow(this, &STimersView::TreeView_OnGenerateRow)
					.OnSelectionChanged(this, &STimersView::TreeView_OnSelectionChanged)
					.OnMouseButtonDoubleClick(this, &STimersView::TreeView_OnMouseButtonDoubleClick)
					.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &STimersView::TreeView_GetMenuContent))
					.ItemHeight(16.0f)
					.HeaderRow
					(
						SAssignNew(TreeViewHeaderRow, SHeaderRow)
						.Visibility(EVisibility::Visible)
					)
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(16.0f)
				[
					SAssignNew(AsyncOperationStatus, Insights::SAsyncOperationStatus, Aggregator)
				]
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

		// Status bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0.0f)
		[
			SNew(SBorder)
			.BorderImage(FInsightsStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.1f, 0.2f, 1.0f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Margin(FMargin(4.0f, 1.0f, 4.0f, 1.0f))
				.Text(LOCTEXT("EmptyAggregationNote", "-- Select a time region to update the aggregated statistics! --"))
				.ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.5f, 1.0f))
				.Visibility_Lambda([this]() { return Aggregator->IsEmptyTimeInterval() && !Aggregator->IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
			]
		]
	];

	InitializeAndShowHeaderColumns();

	// Create the search filters: text based, type based etc.
	TextFilter = MakeShared<FTimerNodeTextFilter>(FTimerNodeTextFilter::FItemToStringArray::CreateSP(this, &STimersView::HandleItemToStringArray));
	Filters = MakeShared<FTimerNodeFilterCollection>();
	Filters->Add(TextFilter);

	CreateGroupByOptionsSources();
	CreateSortings();

	InitCommandList();

	// Register ourselves with the Insights manager.
	FInsightsManager::Get()->GetSessionChangedEvent().AddSP(this, &STimersView::InsightsManager_OnSessionChanged);
	FInsightsManager::Get()->GetSessionAnalysisCompletedEvent().AddSP(this, &STimersView::InsightsManager_OnSessionAnalysisCompleted);

	// Update the Session (i.e. when analysis session was already started).
	InsightsManager_OnSessionChanged();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> STimersView::TreeView_GetMenuContent()
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	const int32 NumSelectedNodes = SelectedNodes.Num();
	FTimerNodePtr SelectedNode = NumSelectedNodes ? SelectedNodes[0] : nullptr;

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
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList.ToSharedRef());

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

	// Timer options section
	MenuBuilder.BeginSection("TimerOptions", LOCTEXT("ContextMenu_Section_TimerOptions", "Timer Options"));
	{
		auto CanExecute = [NumSelectedNodes, SelectedNode]()
		{
			TSharedPtr<STimingProfilerWindow> Wnd = FTimingProfilerManager::Get()->GetProfilerWindow();
			TSharedPtr<STimingView> TimingView = Wnd.IsValid() ? Wnd->GetTimingView() : nullptr;
			return TimingView.IsValid() && NumSelectedNodes == 1 && SelectedNode.IsValid() && SelectedNode->GetType() != ETimerNodeType::Group;
		};

		// Highlight event
		{
			FUIAction Action_ToggleHighlight;
			Action_ToggleHighlight.CanExecuteAction = FCanExecuteAction::CreateLambda(CanExecute);
			Action_ToggleHighlight.ExecuteAction = FExecuteAction::CreateSP(this, &STimersView::ToggleTimingViewEventFilter, SelectedNode);

			TSharedPtr<STimingProfilerWindow> Wnd = FTimingProfilerManager::Get()->GetProfilerWindow();
			TSharedPtr<STimingView> TimingView = Wnd.IsValid() ? Wnd->GetTimingView() : nullptr;

			if (SelectedNode.IsValid() &&
				SelectedNode->GetType() != ETimerNodeType::Group &&
				TimingView.IsValid() &&
				TimingView->IsFilterByEventType(SelectedNode->GetTimerId()))
			{
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("ContextMenu_StopHighlightEvent", "Stop Highlighting Event"),
					LOCTEXT("ContextMenu_StopHighlightEvent_Desc", "Stops highlighting timing event instances for the selected timer."),
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Visible"),
					Action_ToggleHighlight,
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
			else
			{
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("ContextMenu_HighlightEvent", "Highlight Event"),
					LOCTEXT("ContextMenu_HighlightEvent_Desc", "Highlights all timing event instances for the selected timer."),
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Visible"),
					Action_ToggleHighlight,
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
		}

		// Add/remove series to/from graph track
		{
			FUIAction Action_ToggleTimerInGraphTrack;
			Action_ToggleTimerInGraphTrack.CanExecuteAction = FCanExecuteAction::CreateLambda(CanExecute);
			Action_ToggleTimerInGraphTrack.ExecuteAction = FExecuteAction::CreateSP(this, &STimersView::ToggleTimingViewMainGraphEventSeries, SelectedNode);

			if (SelectedNode.IsValid() &&
				SelectedNode->GetType() != ETimerNodeType::Group &&
				IsSeriesInTimingViewMainGraph(SelectedNode))
			{
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("ContextMenu_RemoveFromGraphTrack", "Remove series from graph track"),
					LOCTEXT("ContextMenu_RemoveFromGraphTrack_Desc", "Removes the series containing event instances of the selected timer from the Main Graph track."),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.RemoveGraphSeries"),
					Action_ToggleTimerInGraphTrack,
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
			else
			{
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("ContextMenu_AddToGraphTrack", "Add series to graph track"),
					LOCTEXT("ContextMenu_AddToGraphTrack_Desc", "Adds a series containing event instances of the selected timer to the Main Graph track."),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.AddGraphSeries"),
					Action_ToggleTimerInGraphTrack,
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
		}

		// Open Source in IDE
		{
			ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
			ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule.GetAccessor();

			FString File;
			uint32 Line = 0;
			bool bIsValidSource = false;
			if (NumSelectedNodes == 1 && SelectedNode.IsValid() && SelectedNode->GetType() != ETimerNodeType::Group)
			{
				bIsValidSource = SelectedNode->GetSourceFileAndLine(File, Line);
			}

			FText ItemLabel = FText::Format(LOCTEXT("ContextMenu_OpenSource", "Open Source in {0}"), SourceCodeAccessor.GetNameText());

			FText ItemToolTip;
			if (bIsValidSource)
			{
				ItemToolTip = FText::Format(LOCTEXT("ContextMenu_OpenSource_Desc1", "Opens the source file of the selected timer in {0}.\n{1} ({2})"),
					SourceCodeAccessor.GetNameText(), FText::FromString(File), FText::AsNumber(Line, &FNumberFormattingOptions::DefaultNoGrouping()));
			}
			else
			{
				ItemToolTip = FText::Format(LOCTEXT("ContextMenu_OpenSource_Desc2", "Opens the source file of the selected timer in {0}."),
					SourceCodeAccessor.GetNameText());
			}

			MenuBuilder.AddMenuEntry(
				FTimersViewCommands::Get().Command_OpenSource,
				NAME_None,
				ItemLabel,
				ItemToolTip,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), SourceCodeAccessor.GetOpenIconName())
			);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Misc", LOCTEXT("ContextMenu_Section_Misc", "Miscellaneous"));
	{
		MenuBuilder.AddMenuEntry
		(
			FTimersViewCommands::Get().Command_CopyToClipboard,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "GenericCommands.Copy")
		);

		MenuBuilder.AddMenuEntry
		(
			FTimersViewCommands::Get().Command_Export,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save")
		);

		MenuBuilder.AddMenuEntry
		(
			FTimersViewCommands::Get().Command_ExportTimingEventsSelection,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save")
		);

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_ExportOptions_SubMenu", "More Export Options"),
			LOCTEXT("ContextMenu_ExportOptions_SubMenu_Desc", "Exports threads, timers and timing events to text files."),
			FNewMenuDelegate::CreateSP(this, &STimersView::TreeView_BuildExportMenu),
			false,
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save")
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::TreeView_BuildSortByMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("SortColumn", LOCTEXT("ContextMenu_Section_SortColumn", "Sort Column"));

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (Column.IsVisible() && Column.CanBeSorted())
		{
			FUIAction Action_SortByColumn
			(
				FExecuteAction::CreateSP(this, &STimersView::ContextMenu_SortByColumn_Execute, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_SortByColumn_CanExecute, Column.GetId()),
				FIsActionChecked::CreateSP(this, &STimersView::ContextMenu_SortByColumn_IsChecked, Column.GetId())
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
			FExecuteAction::CreateSP(this, &STimersView::ContextMenu_SortMode_Execute, EColumnSortMode::Ascending),
			FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Ascending),
			FIsActionChecked::CreateSP(this, &STimersView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Ascending)
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
			FExecuteAction::CreateSP(this, &STimersView::ContextMenu_SortMode_Execute, EColumnSortMode::Descending),
			FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Descending),
			FIsActionChecked::CreateSP(this, &STimersView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Descending)
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

void STimersView::TreeView_BuildViewColumnMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Columns", LOCTEXT("ContextMenu_Section_Columns", "Columns"));

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		FUIAction Action_ToggleColumn
		(
			FExecuteAction::CreateSP(this, &STimersView::ToggleColumnVisibility, Column.GetId()),
			FCanExecuteAction::CreateSP(this, &STimersView::CanToggleColumnVisibility, Column.GetId()),
			FIsActionChecked::CreateSP(this, &STimersView::IsColumnVisible, Column.GetId())
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

void STimersView::TreeView_BuildExportMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Export", LOCTEXT("ContextMenu_Section_Export", "Export"));
	{
		MenuBuilder.AddMenuEntry
		(
			FTimersViewCommands::Get().Command_ExportThreads,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save")
		);

		MenuBuilder.AddMenuEntry
		(
			FTimersViewCommands::Get().Command_ExportTimers,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save")
		);

		MenuBuilder.AddMenuEntry
		(
			FTimersViewCommands::Get().Command_ExportTimingEvents,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Save")
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::InitializeAndShowHeaderColumns()
{
	// Create columns.
	TArray<TSharedRef<Insights::FTableColumn>> Columns;
	FTimersViewColumnFactory::CreateTimersViewColumns(Columns);
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

FText STimersView::GetColumnHeaderText(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.GetShortName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STimersView::TreeViewHeaderRow_GenerateColumnMenu(const Insights::FTableColumn& Column)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("Sorting", LOCTEXT("ContextMenu_Section_Sorting", "Sorting"));
	{
		if (Column.CanBeSorted())
		{
			FUIAction Action_SortAscending
			(
				FExecuteAction::CreateSP(this, &STimersView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Ascending),
				FCanExecuteAction::CreateSP(this, &STimersView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Ascending),
				FIsActionChecked::CreateSP(this, &STimersView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Ascending)
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
				FExecuteAction::CreateSP(this, &STimersView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Descending),
				FCanExecuteAction::CreateSP(this, &STimersView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Descending),
				FIsActionChecked::CreateSP(this, &STimersView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Descending)
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
			FNewMenuDelegate::CreateSP(this, &STimersView::TreeView_BuildSortByMenu),
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
				FExecuteAction::CreateSP(this, &STimersView::HideColumn, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &STimersView::CanHideColumn, Column.GetId())
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
			FNewMenuDelegate::CreateSP(this, &STimersView::TreeView_BuildViewColumnMenu),
			false,
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ViewColumn")
		);

		FUIAction Action_ShowAllColumns
		(
			FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ShowAllColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ShowAllColumns_CanExecute)
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

		FUIAction Action_ShowMinMaxMedColumns
		(
			FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ShowMinMaxMedColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ShowMinMaxMedColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_ShowMinMaxMedColumns", "Reset Columns to Min/Max/Median Preset"),
			LOCTEXT("ContextMenu_ShowMinMaxMedColumns_Desc", "Resets columns to Min/Max/Median preset."),
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ResetColumn"),
			Action_ShowMinMaxMedColumns,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		FUIAction Action_ResetColumns
		(
			FExecuteAction::CreateSP(this, &STimersView::ContextMenu_ResetColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STimersView::ContextMenu_ResetColumns_CanExecute)
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

void STimersView::InsightsManager_OnSessionChanged()
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

void STimersView::InsightsManager_OnSessionAnalysisCompleted()
{
	// Re-sync the list of timers to update the "<unknown>" timer names.
	RebuildTree(true);

	// Aggregate stats automatically for the entire session (but only if user didn't made a time selection yet).
	if (Aggregator->IsEmptyTimeInterval() && !Aggregator->IsRunning())
	{
		TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
		InsightsManager->UpdateSessionDuration();
		const double SessionDuration = InsightsManager->GetSessionDuration();

		constexpr double Delta = 0.0; // session padding

		Aggregator->Cancel();
		Aggregator->SetTimeInterval(0.0 - Delta, SessionDuration + Delta);
		Aggregator->Start();

		if (ColumnBeingSorted == NAME_None)
		{
			// Restore sorting...
			SetSortModeForColumn(GetDefaultColumnBeingSorted(), GetDefaultColumnSortMode());
			TreeView_Refresh();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::UpdateTree()
{
	CreateGroups();
	SortTreeNodes();
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ApplyFiltering()
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	FilteredGroupNodes.Reset();

	// Apply filter to all groups and its children.
	const int32 NumGroups = GroupNodes.Num();
	for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
	{
		FTimerNodePtr& GroupPtr = GroupNodes[GroupIndex];
		GroupPtr->ClearFilteredChildren();
		const bool bIsGroupVisible = Filters->PassesAllFilters(GroupPtr);

		const TArray<Insights::FBaseTreeNodePtr>& GroupChildren = GroupPtr->GetChildren();
		int32 NumVisibleChildren = 0;
		for (const Insights::FBaseTreeNodePtr& ChildPtr : GroupChildren)
		{
			const FTimerNodePtr& NodePtr = StaticCastSharedPtr<FTimerNode, Insights::FBaseTreeNode>(ChildPtr);

			const bool bIsChildVisible = (!bFilterOutZeroCountTimers || NodePtr->GetAggregatedStats().InstanceCount > 0)
									  && bTimerTypeIsVisible[static_cast<int>(NodePtr->GetType())]
									  && Filters->PassesAllFilters(NodePtr);
			if (bIsChildVisible)
			{
				// Add a child.
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

	// Only expand tree nodes if we have a text filter.
	const bool bNonEmptyTextFilter = !TextFilter->GetRawFilterText().IsEmpty();
	if (bNonEmptyTextFilter)
	{
		if (!bExpansionSaved)
		{
			ExpandedNodes.Empty();
			TreeView->GetExpandedItems(ExpandedNodes);
			bExpansionSaved = true;
		}

		for (const FTimerNodePtr& GroupPtr : FilteredGroupNodes)
		{
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

	// Update aggregations for groups.
	for (FTimerNodePtr& GroupPtr : FilteredGroupNodes)
	{
		TraceServices::FTimingProfilerAggregatedStats& AggregatedStats = GroupPtr->GetAggregatedStats();

		GroupPtr->ResetAggregatedStats();

		constexpr double NanTimeValue = std::numeric_limits<double>::quiet_NaN();
		AggregatedStats.AverageInclusiveTime = NanTimeValue;
		AggregatedStats.MedianInclusiveTime = NanTimeValue;
		AggregatedStats.AverageExclusiveTime = NanTimeValue;
		AggregatedStats.MedianExclusiveTime = NanTimeValue;

		const TArray<Insights::FBaseTreeNodePtr>& GroupChildren = GroupPtr->GetFilteredChildren();
		for (const Insights::FBaseTreeNodePtr& ChildPtr : GroupChildren)
		{
			const FTimerNodePtr& NodePtr = StaticCastSharedPtr<FTimerNode, Insights::FBaseTreeNode>(ChildPtr);
			const TraceServices::FTimingProfilerAggregatedStats& NodeAggregatedStats = NodePtr->GetAggregatedStats();

			if (NodeAggregatedStats.InstanceCount > 0)
			{
				AggregatedStats.InstanceCount += NodeAggregatedStats.InstanceCount;
				AggregatedStats.TotalInclusiveTime += NodeAggregatedStats.TotalInclusiveTime;
				AggregatedStats.MinInclusiveTime = FMath::Min(AggregatedStats.MinInclusiveTime, NodeAggregatedStats.MinInclusiveTime);
				AggregatedStats.MaxInclusiveTime = FMath::Max(AggregatedStats.MaxInclusiveTime, NodeAggregatedStats.MaxInclusiveTime);
				AggregatedStats.TotalExclusiveTime += NodeAggregatedStats.TotalExclusiveTime;
				AggregatedStats.MinExclusiveTime = FMath::Min(AggregatedStats.MinExclusiveTime, NodeAggregatedStats.MinExclusiveTime);
				AggregatedStats.MaxExclusiveTime = FMath::Max(AggregatedStats.MaxExclusiveTime, NodeAggregatedStats.MaxExclusiveTime);
			}
		}
	}

	// Request tree refresh
	TreeView->RequestTreeRefresh();

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TimingProfiler, Log, TEXT("[Timers] Tree view filtered in %.3fs (%d timers)"),
			TotalTime, TimerNodes.Num());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::HandleItemToStringArray(const FTimerNodePtr& FTimerNodePtr, TArray<FString>& OutSearchStrings) const
{
	OutSearchStrings.Add(FTimerNodePtr->GetName().GetPlainNameString());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STimersView::GetToggleButtonForTimerType(const ETimerNodeType NodeType)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.Padding(FMargin(4.0f, 2.0f, 4.0f, 2.0f))
		.HAlign(HAlign_Center)
		.OnCheckStateChanged(this, &STimersView::FilterByTimerType_OnCheckStateChanged, NodeType)
		.IsChecked(this, &STimersView::FilterByTimerType_IsChecked, NodeType)
		.ToolTipText(TimerNodeTypeHelper::ToDescription(NodeType))
		[
			SNew(SHorizontalBox)

			//+ SHorizontalBox::Slot()
			//.Padding(0.0f, 0.0f, 2.0f, 0.0f)
			//.AutoWidth()
			//.VAlign(VAlign_Center)
			//[
			//	SNew(SImage)
			//	.Image(TimerNodeTypeHelper::GetIconForTimerNodeType(NodeType))
			//]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(TimerNodeTypeHelper::ToText(NodeType))
			]
		];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::FilterOutZeroCountTimers_OnCheckStateChanged(ECheckBoxState NewRadioState)
{
	bFilterOutZeroCountTimers = (NewRadioState == ECheckBoxState::Checked);
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ECheckBoxState STimersView::FilterOutZeroCountTimers_IsChecked() const
{
	return bFilterOutZeroCountTimers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::FilterByTimerType_OnCheckStateChanged(ECheckBoxState NewRadioState, const ETimerNodeType InStatType)
{
	bTimerTypeIsVisible[static_cast<int>(InStatType)] = (NewRadioState == ECheckBoxState::Checked);
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ECheckBoxState STimersView::FilterByTimerType_IsChecked(const ETimerNodeType InStatType) const
{
	return bTimerTypeIsVisible[static_cast<int>(InStatType)] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeView
////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::TreeView_Refresh()
{
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::TreeView_OnSelectionChanged(FTimerNodePtr SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		TArray<FTimerNodePtr> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() == 1 && SelectedItems[0]->GetType() != ETimerNodeType::Group)
		{
			FTimingProfilerManager::Get()->SetSelectedTimer(SelectedItems[0]->GetTimerId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::TreeView_OnGetChildren(FTimerNodePtr InParent, TArray<FTimerNodePtr>& OutChildren)
{
	const TArray<Insights::FBaseTreeNodePtr>& Children = InParent->GetFilteredChildren();
	OutChildren.Reset(Children.Num());
	for (const Insights::FBaseTreeNodePtr& Child : Children)
	{
		OutChildren.Add(StaticCastSharedPtr<FTimerNode, Insights::FBaseTreeNode>(Child));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::TreeView_OnMouseButtonDoubleClick(FTimerNodePtr NodePtr)
{
	if (NodePtr->GetType() == ETimerNodeType::Group)
	{
		const bool bIsGroupExpanded = TreeView->IsItemExpanded(NodePtr);
		TreeView->SetItemExpansion(NodePtr, !bIsGroupExpanded);
	}
	else
	{
		if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
		{
			ToggleTimingViewEventFilter(NodePtr);
		}
		else
		{
			ToggleTimingViewMainGraphEventSeries(NodePtr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tree View's Table Row
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> STimersView::TreeView_OnGenerateRow(FTimerNodePtr NodePtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> TableRow =
		SNew(STimerTableRow, OwnerTable)
		.OnShouldBeEnabled(this, &STimersView::TableRow_ShouldBeEnabled)
		.OnIsColumnVisible(this, &STimersView::IsColumnVisible)
		.OnSetHoveredCell(this, &STimersView::TableRow_SetHoveredCell)
		.OnGetColumnOutlineHAlignmentDelegate(this, &STimersView::TableRow_GetColumnOutlineHAlignment)
		.HighlightText(this, &STimersView::TableRow_GetHighlightText)
		.HighlightedNodeName(this, &STimersView::TableRow_GetHighlightedNodeName)
		.TablePtr(Table)
		.TimerNodePtr(NodePtr);

	return TableRow;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::TableRow_ShouldBeEnabled(FTimerNodePtr NodePtr) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::TableRow_SetHoveredCell(TSharedPtr<Insights::FTable> InTablePtr, TSharedPtr<Insights::FTableColumn> InColumnPtr, FTimerNodePtr InNodePtr)
{
	HoveredColumnId = InColumnPtr ? InColumnPtr->GetId() : FName();

	const bool bIsAnyMenusVisible = FSlateApplication::Get().AnyMenusVisible();
	if (!HasMouseCapture() && !bIsAnyMenusVisible)
	{
		HoveredNodePtr = InNodePtr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EHorizontalAlignment STimersView::TableRow_GetColumnOutlineHAlignment(const FName ColumnId) const
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

FText STimersView::TableRow_GetHighlightText() const
{
	return SearchBox->GetText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName STimersView::TableRow_GetHighlightedNodeName() const
{
	return HighlightedNodeName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SearchBox
////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::SearchBox_OnTextChanged(const FText& InFilterText)
{
	TextFilter->SetRawFilterText(InFilterText);
	SearchBox->SetError(TextFilter->GetFilterErrorText());
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::SearchBox_IsEnabled() const
{
	return TimerNodes.Num() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Grouping
////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::CreateGroups()
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	if (GroupingMode == ETimerGroupingMode::Flat)
	{
		GroupNodes.Reset();

		const FName GroupName(TEXT("All"));
		FTimerNodePtr GroupPtr = MakeShared<FTimerNode>(GroupName);
		GroupNodes.Add(GroupPtr);

		for (const FTimerNodePtr& NodePtr : TimerNodes)
		{
			GroupPtr->AddChildAndSetGroupPtr(NodePtr);
		}
		TreeView->SetItemExpansion(GroupPtr, true);
	}
	// Creates groups based on stat metadata groups.
	else if (GroupingMode == ETimerGroupingMode::ByMetaGroupName)
	{
		TMap<FName, FTimerNodePtr> GroupNodeSet;
		for (const FTimerNodePtr& NodePtr : TimerNodes)
		{
			const FName GroupName = NodePtr->GetMetaGroupName();
			FTimerNodePtr GroupPtr = GroupNodeSet.FindRef(GroupName);
			if (!GroupPtr)
			{
				GroupPtr = GroupNodeSet.Add(GroupName, MakeShared<FTimerNode>(GroupName));
			}
			GroupPtr->AddChildAndSetGroupPtr(NodePtr);
			TreeView->SetItemExpansion(GroupPtr, true);
		}
		GroupNodeSet.KeySort([](const FName& A, const FName& B) { return A.Compare(B) < 0; }); // sort groups by name
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Creates one group for each stat type.
	else if (GroupingMode == ETimerGroupingMode::ByType)
	{
		TMap<ETimerNodeType, FTimerNodePtr> GroupNodeSet;
		for (const FTimerNodePtr& NodePtr : TimerNodes)
		{
			const ETimerNodeType NodeType = NodePtr->GetType();
			FTimerNodePtr GroupPtr = GroupNodeSet.FindRef(NodeType);
			if (!GroupPtr)
			{
				const FName GroupName = *TimerNodeTypeHelper::ToText(NodeType).ToString();
				GroupPtr = GroupNodeSet.Add(NodeType, MakeShared<FTimerNode>(GroupName));
			}
			GroupPtr->AddChildAndSetGroupPtr(NodePtr);
			TreeView->SetItemExpansion(GroupPtr, true);
		}
		GroupNodeSet.KeySort([](const ETimerNodeType& A, const ETimerNodeType& B) { return A < B; }); // sort groups by type
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Creates one group for one letter.
	else if (GroupingMode == ETimerGroupingMode::ByName)
	{
		TMap<TCHAR, FTimerNodePtr> GroupNodeSet;
		for (const FTimerNodePtr& NodePtr : TimerNodes)
		{
			FString FirstLetterStr(NodePtr->GetName().GetPlainNameString().Left(1).ToUpper());
			const TCHAR FirstLetter = FirstLetterStr[0];
			FTimerNodePtr GroupPtr = GroupNodeSet.FindRef(FirstLetter);
			if (!GroupPtr)
			{
				const FName GroupName(FirstLetterStr);
				GroupPtr = GroupNodeSet.Add(FirstLetter, MakeShared<FTimerNode>(GroupName));
			}
			GroupPtr->AddChildAndSetGroupPtr(NodePtr);
		}
		GroupNodeSet.KeySort([](const TCHAR& A, const TCHAR& B) { return A < B; }); // sort groups alphabetically
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Creates one group for each logarithmic range ie. 0, [1 .. 10), [10 .. 100), [100 .. 1K), etc.
	else if (GroupingMode == ETimerGroupingMode::ByInstanceCount)
	{
		const TCHAR* Orders[] =
		{
			TEXT("1"), TEXT("10"), TEXT("100"),
			TEXT("1K"), TEXT("10K"), TEXT("100K"),
			TEXT("1M"), TEXT("10M"), TEXT("100M"),
			TEXT("1G"), TEXT("10G"), TEXT("100G"),
			TEXT("1T")
		};
		const uint32 MaxOrder = UE_ARRAY_COUNT(Orders);
		TMap<uint32, FTimerNodePtr> GroupNodeSet;
		for (const FTimerNodePtr& NodePtr : TimerNodes)
		{
			uint64 InstanceCount = NodePtr->GetAggregatedStats().InstanceCount;
			uint32 Order = 0;
			while (InstanceCount)
			{
				InstanceCount /= 10;
				Order++;
			}
			FTimerNodePtr GroupPtr = GroupNodeSet.FindRef(Order);
			if (!GroupPtr)
			{
				const FName GroupName =
				    (Order == 0) ?          FName(TEXT("Count == 0")) :
				    (Order < MaxOrder) ?    FName(FString::Printf(TEXT("Count: [%s .. %s)"), Orders[Order - 1], Orders[Order])) :
				                            FName(FString::Printf(TEXT("Count >= %s"), Orders[MaxOrder - 1]));
				GroupPtr = GroupNodeSet.Add(Order, MakeShared<FTimerNode>(GroupName));
			}
			GroupPtr->AddChildAndSetGroupPtr(NodePtr);
		}
		GroupNodeSet.KeySort([](const uint32& A, const uint32& B) { return A > B; }); // sort groups by order
		GroupNodeSet.GenerateValueArray(GroupNodes);
	}
	// Creates one group for each logarithmic range ie. 0.001 - 0.01, 0.01 - 0.1, 0.1 - 1.0, 1.0 - 10.0, etc.
	else if (GroupingMode == ETimerGroupingMode::ByTotalInclusiveTime)
	{
		//im:TODO:
	}
	// Creates one group for each logarithmic range ie. 0.001 - 0.01, 0.01 - 0.1, 0.1 - 1.0, 1.0 - 10.0, etc.
	else if (GroupingMode == ETimerGroupingMode::ByTotalExclusiveTime)
	{
		//im:TODO:
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TimingProfiler, Log, TEXT("[Timers] Tree view grouping updated in %.3fs (%d timers)"),
			TotalTime, TimerNodes.Num());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::CreateGroupByOptionsSources()
{
	GroupByOptionsSource.Reset(3);

	// Must be added in order of elements in the ETimerGroupingMode.
	GroupByOptionsSource.Add(MakeShared<ETimerGroupingMode>(ETimerGroupingMode::Flat));
	GroupByOptionsSource.Add(MakeShared<ETimerGroupingMode>(ETimerGroupingMode::ByName));
	//GroupByOptionsSource.Add(MakeShared<ETimerGroupingMode>(ETimerGroupingMode::ByMetaGroupName));
	GroupByOptionsSource.Add(MakeShared<ETimerGroupingMode>(ETimerGroupingMode::ByType));
	GroupByOptionsSource.Add(MakeShared<ETimerGroupingMode>(ETimerGroupingMode::ByInstanceCount));

	ETimerGroupingModePtr* GroupingModePtrPtr = GroupByOptionsSource.FindByPredicate([&](const ETimerGroupingModePtr InGroupingModePtr) { return *InGroupingModePtr == GroupingMode; });
	if (GroupingModePtrPtr != nullptr)
	{
		GroupByComboBox->SetSelectedItem(*GroupingModePtrPtr);
	}

	GroupByComboBox->RefreshOptions();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::GroupBy_OnSelectionChanged(TSharedPtr<ETimerGroupingMode> NewGroupingMode, ESelectInfo::Type SelectInfo)
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

TSharedRef<SWidget> STimersView::GroupBy_OnGenerateWidget(TSharedPtr<ETimerGroupingMode> InGroupingMode) const
{
	return SNew(STextBlock)
		.Text(TimerNodeGroupingHelper::ToText(*InGroupingMode))
		.ToolTipText(TimerNodeGroupingHelper::ToDescription(*InGroupingMode));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STimersView::GroupBy_GetSelectedText() const
{
	return TimerNodeGroupingHelper::ToText(GroupingMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STimersView::GroupBy_GetSelectedTooltipText() const
{
	return TimerNodeGroupingHelper::ToDescription(GroupingMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorting
////////////////////////////////////////////////////////////////////////////////////////////////////

const FName STimersView::GetDefaultColumnBeingSorted()
{
	return FTimersViewColumns::TotalInclusiveTimeColumnID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const EColumnSortMode::Type STimersView::GetDefaultColumnSortMode()
{
	return EColumnSortMode::Descending;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::CreateSortings()
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

void STimersView::UpdateCurrentSortingByColumn()
{
	TSharedPtr<Insights::FTableColumn> ColumnPtr = Table->FindColumn(ColumnBeingSorted);
	CurrentSorter = ColumnPtr.IsValid() ? ColumnPtr->GetValueSorter() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::SortTreeNodes()
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	if (CurrentSorter.IsValid())
	{
		for (FTimerNodePtr& Root : GroupNodes)
		{
			SortTreeNodesRec(*Root, *CurrentSorter);
		}
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TimingProfiler, Log, TEXT("[Timers] Tree view sorted (%s, %c) in %.3fs (%d timers)"),
			CurrentSorter.IsValid() ? *CurrentSorter->GetShortName().ToString() : TEXT("N/A"),
			(ColumnSortMode == EColumnSortMode::Type::Descending) ? TEXT('D') : TEXT('A'),
			TotalTime, TimerNodes.Num());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::SortTreeNodesRec(FTimerNode& Node, const Insights::ITableCellValueSorter& Sorter)
{
	if (ColumnSortMode == EColumnSortMode::Type::Descending)
	{
		Node.SortChildrenDescending(Sorter);
	}
	else // if (ColumnSortMode == EColumnSortMode::Type::Ascending)
	{
		Node.SortChildrenAscending(Sorter);
	}

#if 0 // Current groupings creates only one level.
	for (Insights::FBaseTreeNodePtr ChildPtr : Node.GetChildren())
	{
		if (ChildPtr->GetChildren().Num() > 0)
		{
			SortTreeNodesRec(*StaticCastSharedPtr<FTimerNode>(ChildPtr), Sorter);
		}
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EColumnSortMode::Type STimersView::GetSortModeForColumn(const FName ColumnId) const
{
	if (ColumnBeingSorted != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return ColumnSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::SetSortModeForColumn(const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	ColumnBeingSorted = ColumnId;
	ColumnSortMode = SortMode;
	UpdateCurrentSortingByColumn();

	SortTreeNodes();
	ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	SetSortModeForColumn(ColumnId, SortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (HeaderMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::HeaderMenu_SortMode_IsChecked(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	return ColumnBeingSorted == ColumnId && ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::HeaderMenu_SortMode_CanExecute(const FName ColumnId, const EColumnSortMode::Type InSortMode) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeSorted();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::HeaderMenu_SortMode_Execute(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnId, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_SortMode_IsChecked(const EColumnSortMode::Type InSortMode)
{
	return ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_SortMode_CanExecute(const EColumnSortMode::Type InSortMode) const
{
	return true; //ColumnSortMode != InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_SortMode_Execute(const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnBeingSorted, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortByColumn action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_SortByColumn_IsChecked(const FName ColumnId)
{
	return ColumnId == ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_SortByColumn_CanExecute(const FName ColumnId) const
{
	return true; //ColumnId != ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_SortByColumn_Execute(const FName ColumnId)
{
	SetSortModeForColumn(ColumnId, EColumnSortMode::Descending);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::CanShowColumn(const FName ColumnId) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ShowColumn(const FName ColumnId)
{
	Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	Column.Show();

	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(Column.GetId())
		.DefaultLabel(Column.GetShortName())
		.ToolTip(STimersViewTooltip::GetColumnTooltip(Column))
		.HAlignHeader(Column.GetHorizontalAlignment())
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill)
		.InitialSortMode(Column.GetInitialSortMode())
		.SortMode(this, &STimersView::GetSortModeForColumn, Column.GetId())
		.OnSort(this, &STimersView::OnSortModeChanged)
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
				.Text(this, &STimersView::GetColumnHeaderText, Column.GetId())
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

bool STimersView::CanHideColumn(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::HideColumn(const FName ColumnId)
{
	Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	Column.Hide();

	TreeViewHeaderRow->RemoveColumn(ColumnId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ToggleColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::IsColumnVisible(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::CanToggleColumnVisibility(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return !Column.IsVisible() || Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ToggleColumnVisibility(const FName ColumnId)
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

bool STimersView::ContextMenu_ShowAllColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ShowAllColumns_Execute()
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

bool STimersView::ContextMenu_ShowMinMaxMedColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ShowMinMaxMedColumns_Execute()
{
	TSet<FName> Preset =
	{
		FTimersViewColumns::NameColumnID,
		//FTimersViewColumns::MetaGroupNameColumnID,
		//FTimersViewColumns::TypeColumnID,
		FTimersViewColumns::InstanceCountColumnID,

		FTimersViewColumns::TotalInclusiveTimeColumnID,
		FTimersViewColumns::MaxInclusiveTimeColumnID,
		FTimersViewColumns::MedianInclusiveTimeColumnID,
		FTimersViewColumns::MinInclusiveTimeColumnID,

		FTimersViewColumns::TotalExclusiveTimeColumnID,
		FTimersViewColumns::MaxExclusiveTimeColumnID,
		FTimersViewColumns::MedianExclusiveTimeColumnID,
		FTimersViewColumns::MinExclusiveTimeColumnID,
	};

	ColumnBeingSorted = FTimersViewColumns::TotalInclusiveTimeColumnID;
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

bool STimersView::ContextMenu_ResetColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ResetColumns_Execute()
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

void STimersView::Reset()
{
	Aggregator->Cancel();
	Aggregator->SetTimeInterval(0.0, 0.0);

	RebuildTree(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Check if we need to update the lists of timers, but not too often.
	static uint64 NextTimestamp = 0;
	const uint64 Time = FPlatformTime::Cycles64();
	if (Time > NextTimestamp)
	{
		RebuildTree(false);

		// 1000 timers --> check each 150ms
		// 10000 timers --> check each 600ms
		// 100000 timers --> check each 5.1s
		const double WaitTimeSec = 0.1 + TimerNodes.Num() / 20000.0;
		const uint64 WaitTime = static_cast<uint64>(WaitTimeSec / FPlatformTime::GetSecondsPerCycle64());
		NextTimestamp = Time + WaitTime;
	}

	Aggregator->Tick(Session, InCurrentTime, InDeltaTime, [this]() { FinishAggregation(); });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::RebuildTree(bool bResync)
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	FStopwatch SyncStopwatch;
	SyncStopwatch.Start();

	if (bResync)
	{
		TimerNodes.Empty();
	}

	const uint32 PreviousNodeCount = TimerNodes.Num();

	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		const uint32 TimerCount = TimerReader->GetTimerCount();
		if (TimerCount != PreviousNodeCount)
		{
			check(TimerCount > PreviousNodeCount);
			TimerNodes.Reserve(TimerCount);

			// Add nodes only for new timers.
			for (uint32 TimerIndex = PreviousNodeCount; TimerIndex < TimerCount; ++TimerIndex)
			{
				const TraceServices::FTimingProfilerTimer& Timer = *(TimerReader->GetTimer(TimerIndex));
				ensure(Timer.Id == TimerIndex);
				const ETimerNodeType Type = Timer.IsGpuTimer ? ETimerNodeType::GpuScope : ETimerNodeType::CpuScope;
				FTimerNodePtr TimerNodePtr = MakeShared<FTimerNode>(Timer.Id, Timer.Name, Type);
				TimerNodePtr->SetDefaultSortOrder(TimerIndex + 1);
				TimerNodes.Add(TimerNodePtr);
			}
			ensure(TimerNodes.Num() == TimerCount);
		}
	}

	SyncStopwatch.Stop();

	if (bResync || TimerNodes.Num() != PreviousNodeCount)
	{
		// Disable sorting if too many items.
		if (TimerNodes.Num() > 10000)
		{
			ColumnBeingSorted = NAME_None;
			ColumnSortMode = GetDefaultColumnSortMode();
			UpdateCurrentSortingByColumn();
		}

		UpdateTree();
		Aggregator->Cancel();
		Aggregator->Start();

		// Save selection.
		TArray<FTimerNodePtr> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		TreeView->RebuildList();

		// Restore selection.
		if (SelectedItems.Num() > 0)
		{
			TreeView->ClearSelection();
			for (FTimerNodePtr& NodePtr : SelectedItems)
			{
				NodePtr = GetTimerNode(NodePtr->GetTimerId());
			}
			SelectedItems.RemoveAll([](const FTimerNodePtr& NodePtr) { return !NodePtr.IsValid(); });
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
		UE_LOG(TimingProfiler, Log, TEXT("[Timers] Tree view rebuilt in %.4fs (sync: %.4fs + update: %.4fs) --> %d timers (%d added)"),
			TotalTime, SyncTime, TotalTime - SyncTime, TimerNodes.Num(), TimerNodes.Num() - PreviousNodeCount);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ResetStats()
{
	Aggregator->Cancel();
	Aggregator->SetTimeInterval(0.0, 0.0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::UpdateStats(double StartTime, double EndTime)
{
	Aggregator->Cancel();
	Aggregator->SetTimeInterval(StartTime, EndTime);
	Aggregator->Start();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::FinishAggregation()
{
	for (const FTimerNodePtr& NodePtr : TimerNodes)
	{
		NodePtr->ResetAggregatedStats();
	}

	ApplyAggregation(Aggregator->GetResultTable());
	Aggregator->ResetResults();

	// Invalidate all tree table rows.
	for (const FTimerNodePtr& NodePtr : TimerNodes)
	{
		TSharedPtr<ITableRow> TableRowPtr = TreeView->WidgetFromItem(NodePtr);
		if (TableRowPtr.IsValid())
		{
			TSharedPtr<STimerTableRow> RowPtr = StaticCastSharedPtr<STimerTableRow, ITableRow>(TableRowPtr);
			RowPtr->InvalidateContent();
		}
	}

	UpdateTree(); // grouping + sorting + filtering

	// Ensure the last selected item is visible.
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	if (SelectedNodes.Num() > 0)
	{
		TreeView->RequestScrollIntoView(SelectedNodes.Last());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ApplyAggregation(TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggregatedStatsTable)
{
	if (AggregatedStatsTable)
	{
		TUniquePtr<TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>> TableReader(AggregatedStatsTable->CreateReader());
		while (TableReader->IsValid())
		{
			const TraceServices::FTimingProfilerAggregatedStats* Row = TableReader->GetCurrentRow();
			FTimerNodePtr TimerNodePtr = GetTimerNode(Row->Timer->Id);
			if (TimerNodePtr)
			{
				TimerNodePtr->SetAggregatedStats(*Row);
			}
			TableReader->NextRow();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimerNodePtr STimersView::GetTimerNode(uint32 TimerId) const
{
	return (TimerId < (uint32)TimerNodes.Num()) ? TimerNodes[TimerId] : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::SelectTimerNode(uint32 TimerId)
{
	FTimerNodePtr NodePtr = GetTimerNode(TimerId);
	if (NodePtr)
	{
		TreeView->SetSelection(NodePtr);
		TreeView->RequestScrollIntoView(NodePtr);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ToggleTimingViewEventFilter(FTimerNodePtr TimerNode) const
{
	TSharedPtr<STimingProfilerWindow> Wnd = FTimingProfilerManager::Get()->GetProfilerWindow();
	TSharedPtr<STimingView> TimingView = Wnd.IsValid() ? Wnd->GetTimingView() : nullptr;

	if (TimingView.IsValid())
	{
		const uint64 EventType = static_cast<uint64>(TimerNode->GetTimerId());
		TimingView->ToggleEventFilterByEventType(EventType);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphTrack> STimersView::GetTimingViewMainGraphTrack() const
{
	TSharedPtr<STimingProfilerWindow> Wnd = FTimingProfilerManager::Get()->GetProfilerWindow();
	TSharedPtr<STimingView> TimingView = Wnd.IsValid() ? Wnd->GetTimingView() : nullptr;

	return TimingView.IsValid() ? TimingView->GetMainTimingGraphTrack() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ToggleGraphSeries(TSharedRef<FTimingGraphTrack> GraphTrack, FTimerNodeRef NodePtr) const
{
	const uint32 TimerId = NodePtr->GetTimerId();
	TSharedPtr<FTimingGraphSeries> Series = GraphTrack->GetTimerSeries(TimerId);
	if (Series.IsValid())
	{
		GraphTrack->RemoveTimerSeries(TimerId);
		GraphTrack->SetDirtyFlag();
		NodePtr->SetAddedToGraphFlag(false);
	}
	else
	{
		GraphTrack->Show();
		Series = GraphTrack->AddTimerSeries(TimerId, NodePtr->GetColor());
		Series->SetName(FText::FromName(NodePtr->GetName()));
		GraphTrack->SetDirtyFlag();
		NodePtr->SetAddedToGraphFlag(true);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::IsSeriesInTimingViewMainGraph(FTimerNodePtr TimerNode) const
{
	TSharedPtr<FTimingGraphTrack> GraphTrack = GetTimingViewMainGraphTrack();

	if (GraphTrack.IsValid())
	{
		const uint32 TimerId = TimerNode->GetTimerId();
		TSharedPtr<FTimingGraphSeries> Series = GraphTrack->GetTimerSeries(TimerId);

		return Series.IsValid();
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ToggleTimingViewMainGraphEventSeries(FTimerNodePtr TimerNode) const
{
	TSharedPtr<FTimingGraphTrack> GraphTrack = GetTimingViewMainGraphTrack();
	if (GraphTrack.IsValid())
	{
		ToggleGraphSeries(GraphTrack.ToSharedRef(), TimerNode.ToSharedRef());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_CopyToClipboard_CanExecute() const
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();

	return SelectedNodes.Num() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_CopyToClipboard_Execute()
{
	if (!Table->IsValid())
	{
		return;
	}

	TArray<Insights::FBaseTreeNodePtr> SelectedNodes;
	for (FTimerNodePtr TimerPtr : TreeView->GetSelectedItems())
	{
		SelectedNodes.Add(TimerPtr);
	}

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	FString ClipboardText;

	if (CurrentSorter.IsValid())
	{
		CurrentSorter->Sort(SelectedNodes, ColumnSortMode == EColumnSortMode::Ascending ? Insights::ESortMode::Ascending : Insights::ESortMode::Descending);
	}

	Table->GetVisibleColumnsData(SelectedNodes, FTimingProfilerManager::Get()->GetLogListingName(), TEXT('\t'), true, ClipboardText);

	if (ClipboardText.Len() > 0)
	{
		FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_Export_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_Export_Execute()
{
	if (!Table->IsValid())
	{
		return;
	}

	TArray<Insights::FBaseTreeNodePtr> SelectedNodes;
	for (FTimerNodePtr TimerPtr : TreeView->GetSelectedItems())
	{
		SelectedNodes.Add(TimerPtr);
	}

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	const FString DialogTitle = LOCTEXT("Export_Title", "Export Aggregated Timer Stats").ToString();
	const FString DefaultFile = TEXT("TimerStats.tsv");
	FString Filename;
	if (!OpenSaveTextFileDialog(DialogTitle, DefaultFile, Filename))
	{
		return;
	}

	IFileHandle* ExportFileHandle = OpenExportFile(*Filename);
	if (!ExportFileHandle)
	{
		return;
	}

	FStopwatch Stopwatch;
	Stopwatch.Start();

	UTF16CHAR BOM = UNICODE_BOM;
	ExportFileHandle->Write((uint8*)&BOM, sizeof(UTF16CHAR));

	TCHAR Separator = TEXT('\t');
	if (Filename.EndsWith(TEXT(".csv")))
	{
		Separator = TEXT(',');
	}
	constexpr TCHAR LineEnd = TEXT('\n');
	constexpr TCHAR QuotationMarkBegin = TEXT('\"');
	constexpr TCHAR QuotationMarkEnd = TEXT('\"');

	TStringBuilder<1024> StringBuilder;

	TArray<TSharedRef<Insights::FTableColumn>> VisibleColumns;
	Table->GetVisibleColumns(VisibleColumns);

	// Write header.
	{
		bool bIsFirstColumn = true;
		for (const TSharedRef<Insights::FTableColumn>& ColumnRef : VisibleColumns)
		{
			if (bIsFirstColumn)
			{
				bIsFirstColumn = false;
			}
			else
			{
				StringBuilder.AppendChar(Separator);
			}
			FString Value = ColumnRef->GetShortName().ToString().ReplaceCharWithEscapedChar();
			int32 CharIndex;
			if (Value.FindChar(Separator, CharIndex))
			{
				StringBuilder.AppendChar(QuotationMarkBegin);
				StringBuilder.Append(Value);
				StringBuilder.AppendChar(QuotationMarkEnd);
			}
			else
			{
				StringBuilder.Append(Value);
			}
		}
		StringBuilder.AppendChar(LineEnd);
		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(TCHAR));
	}

	if (CurrentSorter.IsValid())
	{
		CurrentSorter->Sort(SelectedNodes, ColumnSortMode == EColumnSortMode::Ascending ? Insights::ESortMode::Ascending : Insights::ESortMode::Descending);
	}

	const int32 NodeCount = SelectedNodes.Num();
	for (int32 Index = 0; Index < NodeCount; Index++)
	{
		const Insights::FBaseTreeNodePtr& Node = SelectedNodes[Index];

		StringBuilder.Reset();

		bool bIsFirstColumn = true;
		for (const TSharedRef<Insights::FTableColumn>& ColumnRef : VisibleColumns)
		{
			if (bIsFirstColumn)
			{
				bIsFirstColumn = false;
			}
			else
			{
				StringBuilder.AppendChar(Separator);
			}

			FString Value = ColumnRef->GetValueAsSerializableString(*Node).ReplaceCharWithEscapedChar();
			int32 CharIndex;
			if (Value.FindChar(Separator, CharIndex))
			{
				StringBuilder.AppendChar(QuotationMarkBegin);
				StringBuilder.Append(Value);
				StringBuilder.AppendChar(QuotationMarkEnd);
			}
			else
			{
				StringBuilder.Append(Value);
			}
		}
		StringBuilder.AppendChar(LineEnd);
		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(TCHAR));
	}

	ExportFileHandle->Flush();
	delete ExportFileHandle;
	ExportFileHandle = nullptr;

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported aggregated timer stats to file in %.3fs (\"%s\")."), TotalTime, *Filename);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_ExportTimingEventsSelection_CanExecute() const
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();

	return SelectedNodes.Num() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::AddTimerNodeRecursive(FTimerNodePtr InNode, TSet<uint32>& InOutIncludedTimers) const
{
	if (InNode->GetType() == ETimerNodeType::Group)
	{
		for (Insights::FBaseTreeNodePtr ChildNode : InNode->GetFilteredChildren())
		{
			AddTimerNodeRecursive(StaticCastSharedPtr<FTimerNode>(ChildNode), InOutIncludedTimers);
		}
	}
	else
	{
		InOutIncludedTimers.Add(InNode->GetTimerId());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ExportTimingEventsSelection_Execute() const
{
	if (!Session.IsValid())
	{
		return;
	}

	const FString DialogTitle = LOCTEXT("ExportTimingEventsSelection_Title", "Export Timing Events (Selection)").ToString();
	const FString DefaultFile = TEXT("TimingEvents.tsv");
	FString Filename;
	if (!OpenSaveTextFileDialog(DialogTitle, DefaultFile, Filename))
	{
		return;
	}

	Insights::FTimingExporter Exporter(*Session.Get());
	Insights::FTimingExporter::FExportTimingEventsParams Params; // default columns, all timing events

	////////////////////////////////////////////////////////////////////////////////////////////////////

	TSharedPtr<STimingProfilerWindow> Wnd = FTimingProfilerManager::Get()->GetProfilerWindow();
	TSharedPtr<STimingView> TimingView = Wnd.IsValid() ? Wnd->GetTimingView() : nullptr;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Filter by thread (visible Gpu/Cpu tracks in TimingView).

	// These variables needs to be in the same scope with the call to Exporter.ExportTimingEventsAsText()
	// (becasue they are referenced in the ThreadFilter lambda function).
	TSet<uint32> IncludedThreads;
	TSet<uint32> ExcludedThreads;

	if (TimingView.IsValid())
	{
		// Add available Gpu threads to the ExcludedThreads list.
		ExcludedThreads.Add(FGpuTimingTrack::Gpu1ThreadId);
		ExcludedThreads.Add(FGpuTimingTrack::Gpu2ThreadId);

		// Add available Cpu threads to the ExcludedThreads list.
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(*Session.Get());
			ThreadProvider.EnumerateThreads(
				[&ExcludedThreads](const TraceServices::FThreadInfo& ThreadInfo)
				{
					ExcludedThreads.Add(ThreadInfo.Id);
				});
		}

		// Move the threads corresponding to visible Cpu/Gpu tracks to the IncludedThreads list.
		TimingView->EnumerateAllTracks([&IncludedThreads, &ExcludedThreads](TSharedPtr<FBaseTimingTrack>& Track) -> bool
			{
				if (Track->IsVisible() && Track->Is<FThreadTimingTrack>())
				{
					const uint32 ThreadId = Track->As<FThreadTimingTrack>().GetThreadId();
					ExcludedThreads.Remove(ThreadId);
					IncludedThreads.Add(ThreadId);
				}
				return true;
			});

		if (IncludedThreads.Num() < ExcludedThreads.Num())
		{
			Params.ThreadFilter = Insights::FTimingExporter::MakeThreadFilterInclusive(IncludedThreads);
		}
		else
		{
			Params.ThreadFilter = Insights::FTimingExporter::MakeThreadFilterExclusive(ExcludedThreads);
		}
	}

	// Debug/test filters.
	//Params.ThreadFilter = [](uint32 ThreadId) -> bool { return ThreadId == 2; };

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Filter by timing event (e.g.: by timer, by duration, by depth).

	// This variable needs to be in the same scope with the call to Exporter.ExportTimingEventsAsText()
	// (becasue it is referenced in the TimingEventFilter lambda function).
	TSet<uint32> IncludedTimers;

	TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	for (FTimerNodePtr Node : SelectedNodes)
	{
		AddTimerNodeRecursive(Node, IncludedTimers);
	}

	Params.TimingEventFilter = Insights::FTimingExporter::MakeTimingEventFilterByTimersInclusive(IncludedTimers);

	// Debug/test filters.
	//Params.TimingEventFilter = [](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool { return Event.TimerIndex < 100; };
	//Params.TimingEventFilter = [](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool { return EventEndTime - EventStartTime > 0.001; };

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Limit the time interval for enumeration (if a time range selection is made in Timing view).

	Params.IntervalStartTime = -std::numeric_limits<double>::infinity();
	Params.IntervalEndTime = +std::numeric_limits<double>::infinity();
	if (TimingView.IsValid())
	{
		const double SelectionStartTime = TimingView->GetSelectionStartTime();
		const double SelectionEndTime = TimingView->GetSelectionEndTime();
		if (SelectionStartTime < SelectionEndTime)
		{
			Params.IntervalStartTime = SelectionStartTime;
			Params.IntervalEndTime = SelectionEndTime;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////

	Exporter.ExportTimingEventsAsText(*Filename, Params);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_ExportTimingEvents_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ExportTimingEvents_Execute() const
{
	if (!Session.IsValid())
	{
		return;
	}

	const FString DialogTitle = LOCTEXT("ExportTimingEvents_Title", "Export Timing Events (All)").ToString();
	const FString DefaultFile = TEXT("TimingEvents.tsv");
	FString Filename;
	if (!OpenSaveTextFileDialog(DialogTitle, DefaultFile, Filename))
	{
		return;
	}

	Insights::FTimingExporter Exporter(*Session.Get());
	Insights::FTimingExporter::FExportTimingEventsParams Params; // default columns, all timing events
	Exporter.ExportTimingEventsAsText(*Filename, Params);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_ExportThreads_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ExportThreads_Execute() const
{
	if (!Session.IsValid())
	{
		return;
	}

	const FString DialogTitle = LOCTEXT("ExportThreads_Title", "Export Threads").ToString();
	const FString DefaultFile = TEXT("Threads.tsv");
	FString Filename;
	if (!OpenSaveTextFileDialog(DialogTitle, DefaultFile, Filename))
	{
		return;
	}

	Insights::FTimingExporter Exporter(*Session.Get());
	Insights::FTimingExporter::FExportThreadsParams Params; // default
	Exporter.ExportThreadsAsText(*Filename, Params);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_ExportTimers_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_ExportTimers_Execute() const
{
	if (!Session.IsValid())
	{
		return;
	}

	const FString DialogTitle = LOCTEXT("ExportTimers_Title", "Export Timers").ToString();
	const FString DefaultFile = TEXT("Timers.tsv");
	FString Filename;
	if (!OpenSaveTextFileDialog(DialogTitle, DefaultFile, Filename))
	{
		return;
	}

	Insights::FTimingExporter Exporter(*Session.Get());
	Insights::FTimingExporter::FExportTimersParams Params; // default
	Exporter.ExportTimersAsText(*Filename, Params);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::OpenSaveTextFileDialog(const FString& InDialogTitle, const FString& InDefaultFile, FString& OutFilename) const
{
	TArray<FString> SaveFilenames;
	bool bDialogResult = false;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		const FString DefaultPath = FPaths::ProjectSavedDir();
		bDialogResult = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			InDialogTitle,
			DefaultPath,
			InDefaultFile,
			TEXT("Comma-Separated Values (*.csv)|*.csv|Tab-Separated Values (*.tsv)|*.tsv|Text Files (*.txt)|*.txt|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			SaveFilenames
		);
	}

	if (!bDialogResult || SaveFilenames.Num() == 0)
	{
		return false;
	}

	OutFilename = SaveFilenames[0];
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

IFileHandle* STimersView::OpenExportFile(const TCHAR* InFilename) const
{
	IFileHandle* ExportFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(InFilename);

	if (ExportFileHandle == nullptr)
	{
		FName LogListingName = FTimingProfilerManager::Get()->GetLogListingName();
		FMessageLog ReportMessageLog((LogListingName != NAME_None) ? LogListingName : TEXT("Other"));
		ReportMessageLog.Error(LOCTEXT("FailedToOpenFile", "Export failed. Failed to open file for write."));
		ReportMessageLog.Notify();
		return nullptr;
	}

	return ExportFileHandle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimersView::ContextMenu_OpenSource_CanExecute() const
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	const int32 NumSelectedNodes = SelectedNodes.Num();
	if (NumSelectedNodes == 1)
	{
		FTimerNodePtr SelectedNode = SelectedNodes[0];
		if (SelectedNode.IsValid() && SelectedNode->GetType() != ETimerNodeType::Group)
		{
			FString File;
			uint32 Line = 0;
			return SelectedNode->GetSourceFileAndLine(File, Line);
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::ContextMenu_OpenSource_Execute() const
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	const int32 NumSelectedNodes = SelectedNodes.Num();
	if (NumSelectedNodes == 1)
	{
		OpenSourceFileInIDE(SelectedNodes[0]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimersView::OpenSourceFileInIDE(FTimerNodePtr InNode) const
{
	if (InNode.IsValid() && InNode->GetType() != ETimerNodeType::Group)
	{
		FString File;
		uint32 Line = 0;
		bool bIsValidSource = InNode->GetSourceFileAndLine(File, Line);
		if (bIsValidSource)
		{
			ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
			if (FPaths::FileExists(File))
			{
				ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule.GetAccessor();
				SourceCodeAccessor.OpenFileAtLine(File, Line);
			}
			else
			{
				SourceCodeAccessModule.OnOpenFileFailed().Broadcast(File);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply STimersView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return CommandList->ProcessCommandBindings(InKeyEvent) == true ? FReply::Handled() : FReply::Unhandled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
